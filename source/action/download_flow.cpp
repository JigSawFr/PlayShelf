// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/download_flow.hpp"

#include <algorithm>
#include <atomic>
#include <aurore.hpp>
#include <aurore/cadran/ui/cadran_frame.hpp>
#include <chrono>
#include <dirent.h>
#include <map>
#include <memory>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "action/download_job.hpp"
#include "action/library_flow.hpp"
#include "action/rebuilt.hpp"
#include "action/safety_logic.hpp"
#include "action/safety_probe.hpp"
#include "action/screen_flow.hpp"
#include "core/awake.h"
#include "core/sim_env.h"
#include "download/recovery.hpp"
#include "install/console_core.hpp"
#include "install/installer.hpp"
#include "install/prefix_source.hpp"
#include "lantern_app/package.hpp"
#include "lantern_app/range_source.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/cpu_boost.hpp"
#include "util/crypto_backend.hpp"
#include "util/http.hpp"
#include "util/listeners.hpp"
#include "util/paths.hpp"
#include "util/patches.hpp"

using namespace aurore::literals;

namespace download_flow
{
namespace
{
using download_job::Control;
using download_job::Outcome;

// The UI thread's only.
std::vector<download::Job>     s_jobs;
Listeners                      s_listeners;
bool                           s_loaded = false;
bool                           s_patches_checked = false, s_safety_checked = false, s_asking = false;
std::string                    s_running;   // the running job's id
std::shared_ptr<Control>       s_control;
Live                           s_live;
download::Rate                 s_rate;
double                         s_last_byte = 0;   // when the running job's count last went on
bool                           s_ticking = false;

// A package fetched ahead (config prefetch) into ahead_path(): how much of
// it is there, while a worker fetches more.
struct Ahead
{
    std::shared_ptr<std::atomic<bool>> stop = std::make_shared<std::atomic<bool>>(false);
    bool                               running = false;
    bool                               drop = false;   // its job removed: the file goes once the worker ends
    bool                               failed = false; // could not (no room, the server refused): not tried again this run
    uint64_t                           have = 0, total = 0;
};
std::map<std::string, Ahead> s_ahead;    // by job id
std::string                  s_starting; // a job about to run, waiting for its fetch ahead to stop

std::string ahead_dir()
{
    return paths::cache_dir() + "/prefetch";
}

std::string ahead_path(const std::string& id)
{
    return ahead_dir() + "/" + id + ".part";
}

// The room where the files fetched ahead go: the SD card, the installs'.
uint64_t sd_free()
{
    paths::ensure_dir(ahead_dir());
    struct statvfs fs;
    return statvfs(ahead_dir().c_str(), &fs) == 0 ? (uint64_t)fs.f_bavail * fs.f_frsize : 0;
}

double now_seconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

download::Job* find(const std::string& id)
{
    for (download::Job& j : s_jobs)
        if (j.id == id) return &j;
    return nullptr;
}

void changed()
{
    show_pill();
    s_listeners.notify();
}

void save()
{
    const std::vector<download::Job> copy = s_jobs;
    ui::write_later("queue", [copy]() {
        std::string why;
        if (!download::save_queue(copy, &why)) aurore::Logger::warning("downloads: the queue could not be saved: {}", why);
    }, download::queue_file());
}

void awake(bool on)
{
    core_keep_awake(on && config::get().keep_awake);
    screen_flow::active(on && config::get().keep_awake);
}

// The job's server, when it is still there.
std::optional<library::Server> server_for(const download::Job& job)
{
    if (job.server_id.empty()) return std::nullopt;
    const std::vector<library::Server> servers = library::load_servers();
    const library::Server*             s = library::find_server(servers, job.server_id);
    return s ? std::optional<library::Server>(*s) : std::nullopt;
}

void start_next();
void tick();

// The jobs whose time to be tried again came: waiting again.
void retry_due()
{
    const double now = now_seconds();
    bool         any = false;
    for (download::Job& j : s_jobs)
        if (j.state == download::State::Failed && j.retry_at > 0 && j.retry_at <= now) {
            aurore::Logger::info("downloads: {} tried again ({} of {})", j.name, j.attempts, download::RETRIES);
            j.retry_at = 0;
            j.state = download::State::Waiting;
            any = true;
        }
    if (!any) return;
    save();
    changed();
    start_next();
}

// Every second while a job runs or waits to be tried again: the screens
// told (a job standing still, the time before a try), the tries due made.
void tick()
{
    const bool waiting_retry = std::any_of(s_jobs.begin(), s_jobs.end(), [](const download::Job& j) { return j.retry_at > 0; });
    if (s_running.empty() && !waiting_retry) {
        s_ticking = false;
        return;
    }
    s_ticking = true;
    retry_due();
    const bool quiet = !s_running.empty() && now_seconds() - s_last_byte >= download::QUIET_SHOWN;
    if (quiet || waiting_retry) changed();
    aurore::delay(1000, []() { tick(); });
}

void start_ticking()
{
    if (!s_ticking) tick();
}

// What was fetched ahead of a job, forgotten and its file deleted (the
// worker's end does it when one still runs).
void drop_ahead(const std::string& id)
{
    const auto it = s_ahead.find(id);
    if (it != s_ahead.end() && it->second.running) {
        it->second.drop = true;
        *it->second.stop = true;
        return;
    }
    s_ahead.erase(id);
    std::remove(ahead_path(id).c_str());
}

void fetch_ahead();

// A worker's end, on the UI thread.
void ahead_ended(const std::string& id, uint64_t have, uint64_t total, const std::string& why)
{
    Ahead& a = s_ahead[id];
    a.running = false;
    a.have = have;
    if (total) a.total = total;
    if (a.drop || !find(id)) {
        drop_ahead(id);
    } else if (!why.empty() && why != "cancelled") {
        aurore::Logger::warning("downloads: fetching ahead of {} stopped: {}", find(id)->name, why);
        a.failed = true;
    }
    changed();
    if (s_starting == id) {
        s_starting.clear();
        start_next();
    } else {
        fetch_ahead();
    }
}

// While a job runs: the next waiting ones (config prefetch) fetched into
// files of the cache, each by a worker of its own, while the SD card has
// room for them and for their install.
void fetch_ahead()
{
    const int count = config::get().prefetch;
    if (count <= 0 || s_running.empty()) return;
    const uint64_t reserve = s_live.total > s_live.done ? s_live.total - s_live.done : 0;
    for (size_t i : download::ahead_jobs(s_jobs, s_running, count)) {
        const download::Job& job = s_jobs[i];
        Ahead&               a = s_ahead[job.id];
        if (a.running || a.failed || (a.total && a.have >= a.total)) continue;
        a.running = true;
        a.stop = std::make_shared<std::atomic<bool>>(false);
        const auto server = server_for(job);
        if (!server || job.size == 0 || job.sha256.empty()) {
            a.running = false;
            a.failed = true;
            continue;
        }
        const std::string id = job.id, path = ahead_path(job.id);
        const lantern_app::PackageRequest request{job.url, job.size, job.sha256, {}};
        const auto        stop = a.stop;
        aurore::Logger::info("downloads: fetching {} ahead", job.name);
        ui::in_background("fetch ahead", [id, server = *server, request, path, stop, reserve]() {
            const uint64_t total = request.size;
            uint64_t       have = download_job::file_size(path);
            if (have > total) {   // another package's: started over
                std::remove(path.c_str());
                have = 0;
            }
            if (have < total && !download::room_ahead(sd_free(), total - have, total, reserve)) {
                aurore::sync([id, have, total]() { ahead_ended(id, have, total, "no room on the SD card"); });
                return;
            }
            std::string why;
            if (have < total) {
                lantern_app::Platform platform;
                auto                  last = std::chrono::steady_clock::now();
                const auto            got = lantern_app::fetch_package(platform, server, request, path, stop.get(), [&](uint64_t n) {
                    const auto t = std::chrono::steady_clock::now();
                    if (t - last < std::chrono::milliseconds(500)) return;
                    last = t;
                    aurore::sync([id, n, total]() {
                        Ahead& now = s_ahead[id];
                        now.have = n;
                        now.total = total;
                        changed();
                    });
                });
                if (!got.ok) why = got.stopped ? "cancelled" : got.why;
            }
            have = download_job::file_size(path);
            if (have >= total) why.clear();
            aurore::sync([id, have, total, why]() { ahead_ended(id, have, total, why); });
        });
    }
}

void finished(const std::string& id, const Outcome& o)
{
    s_running.clear();
    s_control.reset();
    s_live = Live{};
    s_rate.reset();
    download::Job* j = find(id);
    // What was fetched ahead of it: of no use once it is installed or given
    // up (a content that failed may be the file's fault); kept while it may
    // take up again.
    if (o.ok || o.cancelled || o.undone) drop_ahead(id);
    if (j && o.cancelled) {
        std::erase_if(s_jobs, [&](const download::Job& x) { return x.id == id; });
    } else if (j && o.ok) {
        aurore::Logger::info("downloads: {} installed", j->name);
        rebuilt::note(o.programs);
        j->state = download::State::Done;
        j->journal.reset();
        j->done = j->total;
        j->attempts = 0;
        ui::notify(aurore::getStr("playshelf/downloads/done_toast", j->name), ui::Toast::Done);
    } else if (j && o.paused) {
        j->state = download::State::Paused;
    } else if (j) {
        aurore::Logger::warning("downloads: {} failed: {}", j->name, o.why);
        j->state = download::State::Failed;
        j->error = o.why;
        j->unsigned_content = o.unsigned_content;
        j->compressed_content = o.compressed_content;
        // The install undid what its runs did: nothing left to take up.
        if (o.undone) j->journal.reset();
        const auto delay = download::retry_delay(download::cause_of(o.why, o.unsigned_content), j->attempts);
        if (delay) {
            j->attempts++;
            j->retry_at = now_seconds() + *delay;
            aurore::Logger::info("downloads: {} tried again in {} s", j->name, *delay);
        } else {
            ui::notify(aurore::getStr("playshelf/downloads/failed_toast", j->name), ui::Toast::Refused);
        }
        start_ticking();
    }
    save();
    changed();
    // What the console has: read again for the Library and Catalogue tabs' covers.
    library_flow::reread_installed([]() { changed(); });
    start_next();
}

void run(download::Job& job)
{
    job.state = download::State::Running;
    job.error.clear();
    job.unsigned_content = false;
    job.compressed_content = false;
    s_running = job.id;
    s_control = std::make_shared<Control>();
    s_live = Live{};
    s_live.id = job.id;
    s_live.done = job.done;
    s_live.total = job.total;
    s_rate.reset();
    s_last_byte = now_seconds();
    awake(true);
    start_ticking();

    const auto server = server_for(job);
    const download::Job              copy = job;
    const std::vector<download::Job> snapshot = s_jobs;
    const auto                       ctl = s_control;
    aurore::Logger::info("downloads: {} started", job.name);
    ui::in_background("download", [copy, server, snapshot, ctl]() {
        Outcome o;
        if (server) {
            download_job::Hooks hooks;
            // On the UI thread, from the worker: how far, and what to keep.
            hooks.progress = [id = copy.id](uint64_t done, uint64_t total) {
                if (s_running != id) return;
                if (done > s_live.done) s_last_byte = now_seconds();
                s_live.done = done;
                s_live.total = total;
                s_rate.add(done, now_seconds());
                if (download::Job* j = find(id)) {
                    j->done = s_live.done;
                    j->total = s_live.total;
                }
                changed();
            };
            hooks.journal = [id = copy.id](const install::Journal& jn) {
                if (download::Job* j = find(id)) {
                    j->journal = jn;
                    save();
                }
            };
            o = download_job::run(copy, *server, snapshot, ctl, ahead_path(copy.id), hooks);
        }
        else o.why = download::SERVER_GONE;
        aurore::sync([id = copy.id, o]() { finished(id, o); });
    });
    save();
    changed();
    fetch_ahead();
}

void start_next()
{
    if (!s_running.empty() || s_asking) return;
    const long i = download::next_job(s_jobs);
    if (i < 0) {
        awake(false);
        return;
    }
    // Once a run: the console patches signature checks, or the user goes on.
    if (!s_patches_checked) {
        s_patches_checked = true;
        if (patches::status(ui::patch_report()) == patches::Status::None) {
            s_asking = true;
            ui::confirm(
                "playshelf/downloads/no_patches_body"_i18n, "playshelf/downloads/no_patches_go"_i18n,
                []() {
                    s_asking = false;
                    start_next();
                },
                []() {
                    s_asking = false;
                    pause_all();
                });
            return;
        }
    }
    // Once a run, before the first install: the console is looked at (emuMMC, the names of Nintendo's servers),
    // and the user told when it is at risk. What is not known says nothing.
    if (!s_safety_checked) {
        s_safety_checked = true;
        s_asking = true;
        ui::in_background("safety", []() {
            const safety_logic::Probe probe = safety_probe::run();
            aurore::sync([probe]() {
                if (!safety_logic::risky(probe)) {
                    s_asking = false;
                    start_next();
                    return;
                }
                std::string reasons;
                if (probe.emummc == safety_logic::Answer::No) reasons += "playshelf/downloads/risk_sysmmc"_i18n;
                if (probe.nintendo_blocked == safety_logic::Answer::No) {
                    if (!reasons.empty()) reasons += "\n";
                    reasons += "playshelf/downloads/risk_online"_i18n;
                }
                ui::confirm(
                    aurore::getStr("playshelf/downloads/risk_body", reasons), "playshelf/downloads/no_patches_go"_i18n,
                    []() {
                        s_asking = false;
                        start_next();
                    },
                    []() {
                        s_asking = false;
                        pause_all();
                    });
            });
        });
        return;
    }
    download::Job& job = s_jobs[(size_t)i];
    // What can no longer be done as it stands: its server removed, its
    // package on the console by now (another installer, an earlier job).
    if (!job.server_id.empty()) {
        const std::vector<library::Server> servers = library::load_servers();
        if (!library::find_server(servers, job.server_id)) {
            aurore::Logger::warning("downloads: {}: its server is no longer there", job.name);
            job.state = download::State::Failed;
            job.error = download::SERVER_GONE;
            save();
            changed();
            start_next();
            return;
        }
    }
    if (download::installed_already(job, *library_flow::installed())) {
        aurore::Logger::info("downloads: {} is on the console already", job.name);
        job.state = download::State::Done;
        job.journal.reset();
        job.done = job.total;
        ui::notify(aurore::getStr("playshelf/downloads/already_toast", job.name), ui::Toast::Done);
        save();
        changed();
        start_next();
        return;
    }
    // Its fetch ahead stopped first: the install reads its file.
    if (const auto a = s_ahead.find(job.id); a != s_ahead.end() && a->second.running) {
        s_starting = job.id;
        *a->second.stop = true;
        return;
    }
    run(job);
}

// A job that is not running, removed: what its runs registered deleted.
void undo_stopped(const download::Job& job)
{
    if (!job.journal) return;
    const install::Journal jn = *job.journal;
    ui::in_background("download undo", [jn]() {
        install::CoreConsole console(CoreStorage_Sd);
        if (!console.ok()) return;
        for (const auto& id : jn.registered) console.delete_content(id);
        if (jn.writing) console.delete_placeholder(*jn.writing);
        aurore::sync([]() { library_flow::reread_installed([]() { changed(); }); });
    });
}
}   // namespace

void init()
{
    if (s_loaded) return;
    s_loaded = true;
    s_jobs = download::load_queue();
    // The files fetched ahead: those of jobs still there taken up, the
    // others deleted.
    if (DIR* d = ::opendir(ahead_dir().c_str())) {
        while (const dirent* e = ::readdir(d)) {
            const std::string name = e->d_name;
            if (name.size() < 6 || name.compare(name.size() - 5, 5, ".part") != 0) continue;
            const std::string id = name.substr(0, name.size() - 5);
            if (find(id)) s_ahead[id].have = download_job::file_size(ahead_path(id));
            else std::remove(ahead_path(id).c_str());
        }
        ::closedir(d);
    }
    if (!s_jobs.empty()) aurore::Logger::info("downloads: {} in the queue, {} not finished", s_jobs.size(), download::pending(s_jobs));
}

const std::vector<download::Job>& jobs()
{
    return s_jobs;
}

Live live()
{
    Live l = s_live;
    if (l.id.empty()) return l;
    l.speed = s_rate.bytes_per_second();
    l.quiet = now_seconds() - s_last_byte;
    l.eta = l.total > l.done ? s_rate.eta(l.total - l.done) : std::nullopt;
    return l;
}

size_t add(std::vector<download::Job> more)
{
    size_t added = 0;
    for (download::Job& j : more) {
        if (s_jobs.size() >= download::MAX_JOBS || download::queued(s_jobs, j.url)) continue;
        if (j.id.empty()) j.id = download::new_job_id();
        j.state = download::State::Waiting;
        s_jobs.push_back(std::move(j));
        added++;
    }
    if (!added) return 0;
    save();
    changed();
    start_next();
    return added;
}

void pause(const std::string& id)
{
    download::Job* j = find(id);
    if (!j) return;
    if (id == s_running && s_control) {
        s_control->pause = true;
        s_control->stop = true;
    } else if (j->state == download::State::Waiting) {
        j->state = download::State::Paused;
        save();
        changed();
    }
}

void resume(const std::string& id)
{
    download::Job* j = find(id);
    if (!j || (j->state != download::State::Paused && j->state != download::State::Failed)) return;
    j->state = download::State::Waiting;
    j->error.clear();
    j->attempts = 0;
    j->retry_at = 0;
    save();
    changed();
    start_next();
}

void accept_unsigned(const std::string& id)
{
    if (download::Job* j = find(id)) {
        j->accept_unsigned = true;
        resume(id);
    }
}

void cancel(const std::string& id)
{
    if (id == s_running && s_control) {
        s_control->cancel = true;
        s_control->stop = true;
        return;
    }
    const auto it = std::find_if(s_jobs.begin(), s_jobs.end(), [&](const download::Job& j) { return j.id == id; });
    if (it == s_jobs.end()) return;
    if (it->state != download::State::Done) undo_stopped(*it);
    drop_ahead(id);
    s_jobs.erase(it);
    save();
    changed();
}

void pause_all()
{
    for (download::Job& j : s_jobs)
        if (j.state == download::State::Waiting || j.retry_at > 0) {
            j.state = download::State::Paused;
            j.retry_at = 0;
        }
    if (!s_running.empty()) pause(s_running);
    for (auto& kv : s_ahead)
        if (kv.second.running) *kv.second.stop = true;
    save();
    changed();
}

void resume_all()
{
    for (download::Job& j : s_jobs)
        if (j.state == download::State::Paused) j.state = download::State::Waiting;
    save();
    changed();
    start_next();
}

void clear_finished()
{
    std::erase_if(s_jobs, [](const download::Job& j) { return j.state == download::State::Done; });
    save();
    changed();
}

double retry_in(const download::Job& job)
{
    return job.retry_at > 0 ? std::max(0.0, job.retry_at - now_seconds()) : -1;
}

std::optional<library::Server> server_of(const download::Job& job)
{
    if (job.server_id.empty()) return std::nullopt;
    const std::vector<library::Server> servers = library::load_servers();
    const library::Server*             s = library::find_server(servers, job.server_id);
    return s ? std::optional<library::Server>(*s) : std::nullopt;
}

namespace
{
// The job's address on its server's other address (LAN ↔ WAN); "" for none.
std::string other_url(const download::Job& job, const library::Server& s)
{
    if (s.lan_url.empty() || s.wan_url.empty()) return "";
    if (library::same_origin(job.url, s.lan_url)) return download::rebase(job.url, s.lan_url, s.wan_url);
    if (library::same_origin(job.url, s.wan_url)) return download::rebase(job.url, s.wan_url, s.lan_url);
    return "";
}
}   // namespace

std::vector<download::Remedy> remedies(const download::Job& job)
{
    download::Situation at;
    if (const auto s = server_of(job)) {
        at.server_known = true;
        at.other_address = !other_url(job, *s).empty();
    }
    return download::remedies(download::cause_of(job.error, job.unsigned_content), at);
}

void use_other_address(const std::string& id)
{
    download::Job* j = find(id);
    if (!j) return;
    const auto        s = server_of(*j);
    const std::string url = s ? other_url(*j, *s) : "";
    if (url.empty()) return;
    aurore::Logger::info("downloads: {} from the server's other address", j->name);
    j->url = url;
    resume(id);
}

void find_again(const std::string& id)
{
    const download::Job* j = find(id);
    const auto           s = j ? server_of(*j) : std::nullopt;
    if (!s) return;
    if (library_flow::loading()) {
        ui::notify("playshelf/downloads/find_busy"_i18n, ui::Toast::Refused);
        return;
    }
    library_flow::forget(s->id);
    library_flow::load(*s, [id](const std::shared_ptr<const library_flow::Loaded>& l, const std::string& why) {
        download::Job* job = find(id);
        if (!job) return;
        if (!l) {
            if (!why.empty()) ui::notify(aurore::getStr("playshelf/downloads/find_failed", why), ui::Toast::Refused);
            return;
        }
        const long at = download::find_again(*job, l->fetch.catalog.items);
        if (at < 0) {
            ui::notify(aurore::getStr("playshelf/downloads/find_none", job->name), ui::Toast::Refused);
            return;
        }
        const library::Item& it = l->fetch.catalog.items[(size_t)at];
        aurore::Logger::info("downloads: {} found again in the catalogue", job->name);
        const bool moved = it.url != job->url;
        // Another file, maybe: what an earlier run kept is of no use.
        if (moved) {
            undo_stopped(*job);
            job->journal.reset();
            job->done = 0;
        }
        job->url = it.url;
        if (it.size) job->size = it.size;
        ui::notify(aurore::getStr(moved ? "playshelf/downloads/found_toast" : "playshelf/downloads/found_same", job->name),
                   moved ? ui::Toast::Done : ui::Toast::Info);
        resume(id);
    });
}

std::optional<std::pair<uint64_t, uint64_t>> ahead_of(const std::string& id)
{
    const auto it = s_ahead.find(id);
    if (it == s_ahead.end() || !it->second.total || !it->second.have) return std::nullopt;
    return std::make_pair(std::min(it->second.have, it->second.total), it->second.total);
}

void prefetch_changed()
{
    // Off: the fetches under way stopped, what they have kept for their
    // jobs; on: started.
    if (config::get().prefetch <= 0) {
        for (auto& kv : s_ahead)
            if (kv.second.running) *kv.second.stop = true;
        return;
    }
    for (auto& kv : s_ahead) kv.second.failed = false;
    fetch_ahead();
}

void keep_awake_changed()
{
    awake(!s_running.empty());
}

int listen(std::function<void()> on_change)
{
    return s_listeners.listen(std::move(on_change));
}

void unlisten(int id)
{
    s_listeners.unlisten(id);
}

std::string summary()
{
    const size_t left = download::pending(s_jobs);
    if (!left) {
        const auto failed = std::count_if(s_jobs.begin(), s_jobs.end(), [](const download::Job& j) { return j.state == download::State::Failed; });
        return failed ? aurore::getStr("playshelf/downloads/summary_failed", failed) : "";
    }
    if (!s_running.empty()) {
        const Live l = live();
        const int  percent = l.total ? (int)std::min<uint64_t>(100, l.done * 100 / l.total) : 0;
        return aurore::getStr("playshelf/downloads/summary_running", left, percent);
    }
    const bool waiting = std::any_of(s_jobs.begin(), s_jobs.end(), [](const download::Job& j) { return j.state == download::State::Waiting; });
    return aurore::getStr(waiting ? "playshelf/downloads/summary_waiting" : "playshelf/downloads/summary_paused", left);
}

void show_pill()
{
    const bool stuck = std::any_of(s_jobs.begin(), s_jobs.end(), [](const download::Job& j) {
        return j.state == download::State::Failed || j.state == download::State::Paused;
    });
    // A job standing still is amber too.
    const bool quiet = !s_running.empty() && now_seconds() - s_last_byte >= download::QUIET_SHOWN;
    cadran::set_pill(summary(), "download", (stuck && s_running.empty()) || quiet ? cadran::Tone::WARN : cadran::Tone::OK);
}

}   // namespace download_flow
