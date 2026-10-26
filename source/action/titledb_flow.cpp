// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/titledb_flow.hpp"

#include <borealis.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <sys/statvfs.h>

#include "install/split_stream.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace titledb_flow
{
namespace
{
constexpr long   PACK_TIMEOUT = 3 * 3600;   // a pack's transfer at most (126 MB on a slow link)
constexpr double TARGET_AGE = 120;         // seconds a signed address is used for

std::mutex                                   s_lock;   // s_db
std::shared_ptr<const titledb::Database>     s_db;
Status                                       s_status;
std::atomic<bool>                            s_stop{ false };
std::map<int, std::function<void()>>         s_listeners;
int                                          s_next = 1;

std::string root()
{
    return paths::data_dir() + "/titledb";
}

std::vector<std::string> dirs_of(const std::vector<std::string>& sources)
{
    std::vector<std::string> out;
    out.reserve(sources.size());
    for (const std::string& url : sources) out.push_back(titledb::source_dir(root(), url));
    return out;
}

std::shared_ptr<const titledb::Database> current()
{
    const std::lock_guard<std::mutex> g(s_lock);
    return s_db;
}

void tell()
{
    const auto listeners = s_listeners;
    for (const auto& [id, fn] : listeners) fn();
}

// On a worker: the folders tidied and opened, the old database dropped.
void reopen(const std::vector<std::string>& sources)
{
    const std::vector<std::string> dirs = dirs_of(sources);
    for (const std::string& d : paths::list_dirs(root())) {
        const std::string dir = root() + "/" + d;
        if (std::find(dirs.begin(), dirs.end(), dir) == dirs.end()) {
            if (!titledb::remove_source(dir)) brls::Logger::warning("titledb: {} not removed (in use)", dir);
        } else
            titledb::prune(dir, false);
    }
    auto db = std::make_shared<titledb::Database>();
    db->open(dirs);
    for (const std::string& s : db->skipped) brls::Logger::warning("titledb: {}", s);
    std::vector<titledb::State> states;
    for (const auto& s : db->sources()) states.push_back(s.state);
    {
        const std::lock_guard<std::mutex> g(s_lock);
        s_db = std::move(db);
    }
    brls::sync([states]() {
        s_status.open = true;
        s_status.opened++;
        s_status.sources = states;
        tell();
    });
}

// A pack fetched as the downloads are (install/split_stream): over the
// connections Downloads › Connections per download sets, each taking the
// next 4 MB piece, the bytes appended in order to the part that a cut left.
// A server that serves no ranges: one request (http::download).
bool fetch_pack(const std::string& url, const std::string& path, uint64_t size, uint64_t from,
                const std::function<void(uint64_t)>& progress, std::string* why)
{
    auto whole = [&]() {
        return http::download(url, path, why, (size_t)size, PACK_TIMEOUT,
                              [from, progress](uint64_t done, uint64_t) { progress(done > from ? done - from : 0); }, from,
                              &s_stop);
    };
    const int connections = config::get().connections;
    if (connections < 2 || size - from < 2 * install::SPLIT_CHUNK) return whole();
    // A release's asset redirects to a signed address, which expires after
    // a few minutes: asked again every TARGET_AGE, not for every piece.
    std::mutex  target_lock;
    std::string target;
    double      target_at = -TARGET_AGE;
    auto        target_now = [&]() {
        const std::lock_guard<std::mutex> g(target_lock);
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - target_at >= TARGET_AGE) {
            std::string to, unused;
            target = http::redirect(url, &to, &unused) && to.rfind("https://", 0) == 0 ? to : url;
            target_at = now;
        }
        return target;
    };
    FILE* f = std::fopen(path.c_str(), from ? "ab" : "wb");
    if (!f) {
        if (why) *why = "cannot create " + path;
        return false;
    }
    uint64_t   got = 0;
    bool       write_failed = false;
    const auto range = [&target_now](uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                                 std::string* w, const std::atomic<bool>* stop) {
        return http::range(target_now(), offset, length, sink, w, {}, stop);
    };
    bool ok = install::split_stream(
        from, size - from, connections, range,
        [&](const uint8_t* data, size_t n) {
            if (std::fwrite(data, 1, n, f) != n) return !(write_failed = true);
            got += n;
            progress(got);
            return true;
        },
        why, &s_stop);
    if (std::fclose(f) != 0 && ok) write_failed = true, ok = false;
    if (write_failed && why) *why = "cannot write the file";
    if (s_stop) {
        if (why) *why = "cancelled";
        return false;
    }
    // Nothing came: no ranges there, the whole file in one request.
    if (!ok && !got && !write_failed) {
        brls::Logger::info("titledb: {} in one request ({})", url, why ? *why : "");
        return whole();
    }
    return ok;
}

titledb::Net console_net()
{
    titledb::Net n;
    n.text = [](const std::string& url, std::string* body, std::string* why) {
        return http::get(url, body, why, 64 * 1024, 20);
    };
    n.file = fetch_pack;
    n.free_space = []() -> uint64_t {
        paths::ensure_dir(root());
        struct statvfs fs;
        return statvfs(root().c_str(), &fs) == 0 ? (uint64_t)fs.f_bavail * fs.f_frsize : 0;
    };
    return n;
}
}   // namespace

void start()
{
    const std::vector<std::string> sources = config::get().title_sources;
    ui::in_background("titledb", [sources]() { reopen(sources); });
}

std::optional<titledb::Title> title(uint64_t id)
{
    const auto db = current();
    return db ? db->title(id) : std::nullopt;
}

bool icon(uint64_t id, std::vector<uint8_t>* bytes, std::string* ext)
{
    const auto db = current();
    return db && db->icon(id, bytes, ext);
}

const Status& status()
{
    return s_status;
}

void update()
{
    if (s_status.running) return;
    const std::vector<std::string> sources = config::get().title_sources;
    s_status.running = true;
    s_status.error.clear();
    s_status.changed = false;
    s_status.source = 0;
    s_status.done = s_status.total = 0;
    s_stop = false;
    tell();
    ui::in_background("titledb update", [sources]() {
        const titledb::Net net = console_net();
        std::string        first;
        bool               changed = false;
        for (size_t i = 0; i < sources.size() && !s_stop && !ui::quitting(); i++) {
            brls::sync([i]() {
                s_status.source = i;
                s_status.done = s_status.total = 0;
                tell();
            });
            std::string why;
            const auto  out = titledb::update(sources[i], titledb::source_dir(root(), sources[i]), net,
                                              [last = 0.0](uint64_t done, uint64_t total) mutable {
                                                  // At most twice a second.
                                                  const double now = (double)brls::getCPUTimeUsec() / 1e6;
                                                  if (now - last < 0.5 && done < total) return;
                                                  last = now;
                                                  brls::sync([done, total]() {
                                                      s_status.done = done;
                                                      s_status.total = total;
                                                      tell();
                                                  });
                                              },
                                              &why);
            if (out == titledb::Outcome::Failed) {
                brls::Logger::warning("titledb: {}: {}", sources[i], why);
                if (first.empty()) first = s_stop ? "cancelled" : why;
            }
            changed |= out == titledb::Outcome::Updated;
        }
        if (changed) reopen(sources);
        brls::sync([first, changed]() {
            s_status.running = false;
            s_status.error = first;
            s_status.changed = changed;
            if (!first.empty()) ui::notify(brls::getStr("playshelf/titledb/failed_toast", first), ui::Toast::Info);
            else ui::notify(changed ? "playshelf/titledb/updated"_i18n : "playshelf/titledb/current"_i18n, ui::Toast::Done);
            tell();
        });
    });
}

void stop()
{
    s_stop = true;
}

void sources_changed()
{
    const std::vector<std::string> sources = config::get().title_sources;
    ui::in_background("titledb", [sources]() { reopen(sources); });
}

int listen(std::function<void()> changed)
{
    const int id = s_next++;
    s_listeners[id] = std::move(changed);
    return id;
}

void unlisten(int id)
{
    s_listeners.erase(id);
}

}   // namespace titledb_flow
