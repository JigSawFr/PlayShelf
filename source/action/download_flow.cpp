// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/download_flow.hpp"

#include <algorithm>
#include <atomic>
#include <borealis.hpp>
#include <chrono>
#include <map>
#include <memory>

#include "action/library_flow.hpp"
#include "core/awake.h"
#include "install/console_core.hpp"
#include "install/http_source.hpp"
#include "install/installer.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/patches.hpp"

using namespace brls::literals;

namespace download_flow
{
namespace
{
// What the UI thread tells the running job.
struct Control
{
    std::atomic<bool> pause{false}, cancel{false};
    std::atomic<bool> stop{false};   // either: the download itself cut at once
};

struct Outcome
{
    bool             ok = false, paused = false, cancelled = false, unsigned_content = false;
    std::string      why;
    uint64_t         total = 0;
};

// The UI thread's only.
std::vector<download::Job>     s_jobs;
std::map<int, std::function<void()>> s_listeners;
int                            s_next_listener = 1;
bool                           s_loaded = false;
bool                           s_patches_checked = false, s_asking = false;
std::string                    s_running;   // the running job's id
std::shared_ptr<Control>       s_control;
Live                           s_live;
download::Rate                 s_rate;

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
    // A copy: a listener may unlisten (a screen closing).
    const auto listeners = s_listeners;
    for (const auto& kv : listeners) kv.second();
}

void save()
{
    const std::vector<download::Job> copy = s_jobs;
    ui::write_later("queue", [copy]() {
        std::string why;
        if (!download::save_queue(copy, &why)) brls::Logger::warning("downloads: the queue could not be saved: {}", why);
    }, download::queue_file());
}

void awake(bool on)
{
    core_keep_awake(on && config::get().keep_awake);
}

// The request headers and the pin for `url`, from its server's profile:
// the credentials for that server's own addresses only, the pin for its
// LAN one.
void credentials_of(const download::Job& job, std::vector<std::string>* headers, std::string* pin)
{
    if (job.server_id.empty()) return;
    const std::vector<library::Server> servers = library::load_servers();
    const library::Server*             s = library::find_server(servers, job.server_id);
    if (!s) return;
    const bool lan = !s->lan_url.empty() && library::same_origin(job.url, s->lan_url);
    const bool wan = !s->wan_url.empty() && library::same_origin(job.url, s->wan_url);
    if (lan || wan) *headers = library::auth_headers(s->username, library::password_of(*s));
    if (lan) *pin = s->pin;
}

// On the worker thread.
Outcome run_job(const download::Job& job, const std::vector<std::string>& headers, const std::string& pin,
                const std::vector<download::Job>& snapshot, const std::shared_ptr<Control>& ctl)
{
    using namespace install;
    Outcome o;
    auto    stopped = [&]() {
        o.paused = ctl->pause.load() && !ctl->cancel.load();
        o.cancelled = ctl->cancel.load();
        return o;
    };
    HttpSource src(job.url, headers, &ctl->stop, pin);
    if (!src.open(&o.why)) return ctl->stop ? stopped() : o;
    const auto pkg = package_open(src, &o.why);
    if (!pkg) return ctl->stop ? stopped() : o;
    CoreConsole console(CoreStorage_Sd);
    if (!console.ok()) {
        o.why = console.error();
        return o;
    }
    // What an install cut short and given up (or another installer) left.
    for (const ContentId& id : download::orphans(console.placeholders(), snapshot)) {
        brls::Logger::info("downloads: an orphan placeholder deleted ({})", content_id_hex(id));
        console.delete_placeholder(id);
    }
    const auto keys = core_keys(&o.why);
    if (!keys) return o;

    const std::string id = job.id;
    auto              last = std::chrono::steady_clock::now();
    InstallCallbacks  cb;
    cb.cancel = &ctl->cancel;
    cb.pause = &ctl->pause;
    cb.progress = [&](uint64_t done, uint64_t total, const std::string&) {
        // PlayShelf quitting: the job pauses, to go on at the next start.
        if (ui::quitting()) {
            ctl->pause = true;
            ctl->stop = true;
        }
        const auto t = std::chrono::steady_clock::now();
        if (t - last < std::chrono::milliseconds(500) && done < total) return;
        last = t;
        brls::sync([id, done, total]() {
            if (s_running != id) return;
            s_live.done = done;
            s_live.total = total;
            s_rate.add(done, now_seconds());
            if (download::Job* j = find(id)) {
                j->done = s_live.done;
                j->total = s_live.total;
            }
            changed();
        });
    };
    cb.journal = [id](const Journal& jn) {
        brls::sync([id, jn]() {
            if (download::Job* j = find(id)) {
                j->journal = jn;
                save();
            }
        });
    };
    Installer inst(src, *pkg, console, *keys,
                   InstallOptions{job.accept_unsigned ? SignaturePolicy::Off : SignaturePolicy::Require, false}, cb);
    if (job.journal) inst.resume_from(*job.journal);
    if (!inst.prepare(&o.why)) return ctl->stop ? stopped() : o;
    o.total = inst.bytes_to_write();
    o.ok = inst.run(&o.why);
    if (!o.ok && (inst.paused() || ctl->cancel)) return stopped();
    o.unsigned_content = !o.ok && o.why.find("not signed by Nintendo") != std::string::npos;
    return o;
}

void start_next();

void finished(const std::string& id, const Outcome& o)
{
    s_running.clear();
    s_control.reset();
    s_live = Live{};
    s_rate.reset();
    download::Job* j = find(id);
    if (j && o.cancelled) {
        s_jobs.erase(std::remove_if(s_jobs.begin(), s_jobs.end(), [&](const download::Job& x) { return x.id == id; }),
                     s_jobs.end());
    } else if (j && o.ok) {
        brls::Logger::info("downloads: {} installed", j->name);
        j->state = download::State::Done;
        j->journal.reset();
        j->done = j->total;
    } else if (j && o.paused) {
        j->state = download::State::Paused;
    } else if (j) {
        brls::Logger::warning("downloads: {} failed: {}", j->name, o.why);
        j->state = download::State::Failed;
        j->error = o.why;
        j->unsigned_content = o.unsigned_content;
    }
    save();
    changed();
    // What the console has: read again for the Library tab's covers.
    library_flow::reread_installed([]() { changed(); });
    start_next();
}

void run(download::Job& job)
{
    job.state = download::State::Running;
    job.error.clear();
    job.unsigned_content = false;
    s_running = job.id;
    s_control = std::make_shared<Control>();
    s_live = Live{};
    s_live.id = job.id;
    s_live.done = job.done;
    s_live.total = job.total;
    s_rate.reset();
    awake(true);

    std::vector<std::string> headers;
    std::string              pin;
    credentials_of(job, &headers, &pin);
    const download::Job              copy = job;
    const std::vector<download::Job> snapshot = s_jobs;
    const auto                       ctl = s_control;
    brls::Logger::info("downloads: {} started", job.name);
    ui::in_background("download", [copy, headers, pin, snapshot, ctl]() {
        const Outcome o = run_job(copy, headers, pin, snapshot, ctl);
        brls::sync([id = copy.id, o]() { finished(id, o); });
    });
    save();
    changed();
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
    run(s_jobs[(size_t)i]);
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
        brls::sync([]() { library_flow::reread_installed([]() { changed(); }); });
    });
}
}   // namespace

void init()
{
    if (s_loaded) return;
    s_loaded = true;
    s_jobs = download::load_queue();
    if (!s_jobs.empty()) brls::Logger::info("downloads: {} in the queue, {} not finished", s_jobs.size(), download::pending(s_jobs));
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
    s_jobs.erase(it);
    save();
    changed();
}

void pause_all()
{
    for (download::Job& j : s_jobs)
        if (j.state == download::State::Waiting) j.state = download::State::Paused;
    if (!s_running.empty()) pause(s_running);
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
    s_jobs.erase(std::remove_if(s_jobs.begin(), s_jobs.end(), [](const download::Job& j) { return j.state == download::State::Done; }),
                 s_jobs.end());
    save();
    changed();
}

void keep_awake_changed()
{
    awake(!s_running.empty());
}

int listen(std::function<void()> on_change)
{
    const int id = s_next_listener++;
    s_listeners[id] = std::move(on_change);
    return id;
}

void unlisten(int id)
{
    s_listeners.erase(id);
}

std::string summary()
{
    const size_t left = download::pending(s_jobs);
    if (!left) return "";
    if (!s_running.empty()) {
        const Live l = live();
        const int  percent = l.total ? (int)std::min<uint64_t>(100, l.done * 100 / l.total) : 0;
        return brls::getStr("playshelf/downloads/summary_running", left, percent);
    }
    const bool waiting = std::any_of(s_jobs.begin(), s_jobs.end(), [](const download::Job& j) { return j.state == download::State::Waiting; });
    return brls::getStr(waiting ? "playshelf/downloads/summary_waiting" : "playshelf/downloads/summary_paused", left);
}

}   // namespace download_flow
