// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/titledb_flow.hpp"

#include <borealis.hpp>
#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <sys/statvfs.h>

#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace titledb_flow
{
namespace
{
constexpr long PACK_TIMEOUT = 3 * 3600;   // a pack's transfer at most (126 MB on a slow link)

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

titledb::Net console_net()
{
    titledb::Net n;
    n.text = [](const std::string& url, std::string* body, std::string* why) {
        return http::get(url, body, why, 64 * 1024, 20);
    };
    n.file = [](const std::string& url, const std::string& path, uint64_t size, uint64_t from,
                const std::function<void(uint64_t)>& progress, std::string* why) {
        return http::download(url, path, why, (size_t)size, PACK_TIMEOUT,
                              [from, progress](uint64_t done, uint64_t) { progress(done > from ? done - from : 0); }, from,
                              &s_stop);
    };
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
