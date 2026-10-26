// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/download_flow.hpp"

#include <algorithm>
#include <atomic>
#include <borealis.hpp>
#include <borealis/cadran/ui/cadran_frame.hpp>
#include <chrono>
#include <map>
#include <memory>

#include "action/library_flow.hpp"
#include "core/awake.h"
#include "download/recovery.hpp"
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
    std::atomic<bool> cut{false};    // the pause is the transfer's, not the user's
};

struct Outcome
{
    bool             ok = false, paused = false, cancelled = false, unsigned_content = false;
    bool             undone = false;   // failed in the install: what every run did undone, the journal of no use
    std::string      why;
    uint64_t         total = 0;
};

// A package's source whose transfer, cut short (the network, a busy
// server), pauses the install instead of failing it: its journal kept, it
// is tried again from there. On the worker thread.
class CutSource : public install::Source
{
  public:
    CutSource(install::Source& src, Control& ctl, std::string* why) : inner(src), control(ctl), reason(why) {}
    uint64_t size() const override { return inner.size(); }
    bool     read(uint64_t offset, uint8_t* out, size_t n, std::string* why) override
    {
        return watch(inner.read(offset, out, n, why), why);
    }
    bool stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                std::string* why) override
    {
        return watch(inner.stream(offset, length, sink, why), why);
    }

  private:
    bool watch(bool ok, const std::string* why)
    {
        if (ok || control.stop || !why) return ok;
        const download::Cause c = download::cause_of(*why);
        if (c == download::Cause::Network || c == download::Cause::Server) {
            *reason = *why;
            control.cut = true;
            control.pause = true;
        }
        return ok;
    }
    install::Source& inner;
    Control&         control;
    std::string*     reason;
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
double                         s_last_byte = 0;   // when the running job's count last went on
bool                           s_ticking = false;

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
Outcome run_job(const download::Job& job, const std::vector<std::string>& headers, const std::string& pin, int connections,
                const std::vector<download::Job>& snapshot, const std::shared_ptr<Control>& ctl)
{
    using namespace install;
    Outcome o;
    auto    stopped = [&]() {
        o.paused = ctl->pause.load() && !ctl->cancel.load();
        o.cancelled = ctl->cancel.load();
        return o;
    };
    std::string cut_why;
    HttpSource  http(job.url, headers, &ctl->stop, pin, connections);
    if (!http.open(&o.why)) return ctl->stop ? stopped() : o;
    CutSource  src(http, *ctl, &cut_why);
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
            if (done > s_live.done) s_last_byte = now_seconds();
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
    // The transfer cut, the user not pausing: a failure to try again, the
    // journal kept.
    if (!o.ok && inst.paused() && ctl->cut && !ctl->stop) {
        o.why = cut_why;
        return o;
    }
    if (!o.ok && (inst.paused() || ctl->cancel)) return stopped();
    o.undone = !o.ok;
    o.unsigned_content = !o.ok && o.why.find("not signed by Nintendo") != std::string::npos;
    return o;
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
            brls::Logger::info("downloads: {} tried again ({} of {})", j.name, j.attempts, download::RETRIES);
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
    brls::delay(1000, []() { tick(); });
}

void start_ticking()
{
    if (!s_ticking) tick();
}

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
        j->attempts = 0;
        ui::notify(brls::getStr("playshelf/downloads/done_toast", j->name), ui::Toast::Done);
    } else if (j && o.paused) {
        j->state = download::State::Paused;
    } else if (j) {
        brls::Logger::warning("downloads: {} failed: {}", j->name, o.why);
        j->state = download::State::Failed;
        j->error = o.why;
        j->unsigned_content = o.unsigned_content;
        // The install undid what its runs did: nothing left to take up.
        if (o.undone) j->journal.reset();
        const auto delay = download::retry_delay(download::cause_of(o.why, o.unsigned_content), j->attempts);
        if (delay) {
            j->attempts++;
            j->retry_at = now_seconds() + *delay;
            brls::Logger::info("downloads: {} tried again in {} s", j->name, *delay);
        } else {
            ui::notify(brls::getStr("playshelf/downloads/failed_toast", j->name), ui::Toast::Refused);
        }
        start_ticking();
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
    s_last_byte = now_seconds();
    awake(true);
    start_ticking();

    std::vector<std::string> headers;
    std::string              pin;
    credentials_of(job, &headers, &pin);
    const download::Job              copy = job;
    const std::vector<download::Job> snapshot = s_jobs;
    const auto                       ctl = s_control;
    const int                        connections = config::get().connections;
    brls::Logger::info("downloads: {} started ({} connections)", job.name, connections);
    ui::in_background("download", [copy, headers, pin, connections, snapshot, ctl]() {
        const Outcome o = run_job(copy, headers, pin, connections, snapshot, ctl);
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
    download::Job& job = s_jobs[(size_t)i];
    // What can no longer be done as it stands: its server removed, its
    // package on the console by now (another installer, an earlier job).
    if (!job.server_id.empty()) {
        const std::vector<library::Server> servers = library::load_servers();
        if (!library::find_server(servers, job.server_id)) {
            brls::Logger::warning("downloads: {}: its server is no longer there", job.name);
            job.state = download::State::Failed;
            job.error = download::SERVER_GONE;
            save();
            changed();
            start_next();
            return;
        }
    }
    if (download::installed_already(job, *library_flow::installed())) {
        brls::Logger::info("downloads: {} is on the console already", job.name);
        job.state = download::State::Done;
        job.journal.reset();
        job.done = job.total;
        ui::notify(brls::getStr("playshelf/downloads/already_toast", job.name), ui::Toast::Done);
        save();
        changed();
        start_next();
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
    brls::Logger::info("downloads: {} from the server's other address", j->name);
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
            if (!why.empty()) ui::notify(brls::getStr("playshelf/downloads/find_failed", why), ui::Toast::Refused);
            return;
        }
        const long at = download::find_again(*job, l->fetch.catalog.items);
        if (at < 0) {
            ui::notify(brls::getStr("playshelf/downloads/find_none", job->name), ui::Toast::Refused);
            return;
        }
        const library::Item& it = l->fetch.catalog.items[(size_t)at];
        brls::Logger::info("downloads: {} found again in the catalogue", job->name);
        const bool moved = it.url != job->url;
        // Another file, maybe: what an earlier run kept is of no use.
        if (moved) {
            undo_stopped(*job);
            job->journal.reset();
            job->done = 0;
        }
        job->url = it.url;
        if (it.size) job->size = it.size;
        ui::notify(brls::getStr(moved ? "playshelf/downloads/found_toast" : "playshelf/downloads/found_same", job->name),
                   moved ? ui::Toast::Done : ui::Toast::Info);
        resume(id);
    });
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
    if (!left) {
        const auto failed = std::count_if(s_jobs.begin(), s_jobs.end(), [](const download::Job& j) { return j.state == download::State::Failed; });
        return failed ? brls::getStr("playshelf/downloads/summary_failed", failed) : "";
    }
    if (!s_running.empty()) {
        const Live l = live();
        const int  percent = l.total ? (int)std::min<uint64_t>(100, l.done * 100 / l.total) : 0;
        return brls::getStr("playshelf/downloads/summary_running", left, percent);
    }
    const bool waiting = std::any_of(s_jobs.begin(), s_jobs.end(), [](const download::Job& j) { return j.state == download::State::Waiting; });
    return brls::getStr(waiting ? "playshelf/downloads/summary_waiting" : "playshelf/downloads/summary_paused", left);
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
