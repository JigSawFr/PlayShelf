// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/insights_index.hpp"

#include <cstdio>
#include <ctime>
#include <memory>
#include <mutex>
#include <sys/stat.h>

#include <aurore.hpp>

#include "library/insights_index.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

namespace insights_index
{

namespace
{
constexpr const char* URL = "https://cdn.jsdelivr.net/gh/JigSawFr/PlayInsights@data/index.json";
constexpr size_t      MAX_BYTES = 64 * 1024;
constexpr long        KEEP = 24 * 3600;   // asked again a day after
constexpr long        RETRY = 3600;       // after a failure, not asked again before

std::mutex                                     s_lock;
std::shared_ptr<const library::InsightsIndex> s_index;   // null: none yet, or it failed
time_t                                         s_asked = 0;

std::string cached()
{
    return paths::cache_dir() + "/insights_index.json";
}

long mtime_age(const std::string& file)
{
    struct stat st;
    if (::stat(file.c_str(), &st) != 0) return -1;
    return (long)(std::time(nullptr) - st.st_mtime);
}

std::shared_ptr<const library::InsightsIndex> parse(const std::string& text)
{
    auto index = std::make_shared<library::InsightsIndex>();
    return index->from_json(text) ? index : nullptr;
}

// Under s_lock: today's index, from the cache when it is a day old at most,
// else fetched; null when there is none.
std::shared_ptr<const library::InsightsIndex> today()
{
    const time_t now = std::time(nullptr);
    if (s_asked && now - s_asked < (s_index ? KEEP : RETRY)) return s_index;
    s_asked = now;
    std::string text;
    const long  old = mtime_age(cached());
    if (old >= 0 && old < KEEP && paths::read_file(cached(), text) && (s_index = parse(text))) return s_index;
    http::Response r;
    std::string    why;
    if (!http::request(URL, {}, "", &r, &why, MAX_BYTES, 30) || r.status != 200) {
        aurore::Logger::debug("insights data: index.json not fetched ({} {})", r.status, why);
        return s_index = nullptr;
    }
    if (!(s_index = parse(r.body))) {
        aurore::Logger::warning("insights data: the fetched index.json is not one");
        return nullptr;
    }
    if (paths::ensure_dir(paths::cache_dir())) paths::atomic_write(cached(), r.body);
    aurore::Logger::info("insights data: index.json, {} files", s_index->size());
    return s_index;
}
}   // namespace

bool unchanged(const std::string& name, const std::string& path)
{
    std::shared_ptr<const library::InsightsIndex> index;
    {
        const std::lock_guard<std::mutex> g(s_lock);
        index = today();
    }
    std::string bytes;
    if (!index || !paths::read_file(path, bytes) || library::must_fetch(index.get(), name, bytes)) return false;
    // A stamp next to the copy: the copy itself is not written again.
    paths::atomic_write(path + ".checked", "");
    aurore::Logger::debug("insights data: {} unchanged", name);
    return true;
}

long age(const std::string& path)
{
    const long fetched = mtime_age(path);
    if (fetched < 0) return -1;
    const long checked = mtime_age(path + ".checked");
    return checked >= 0 && checked < fetched ? checked : fetched;
}

void forget()
{
    const std::lock_guard<std::mutex> g(s_lock);
    s_asked = 0;
    s_index = nullptr;
    ::remove(cached().c_str());
}

}   // namespace insights_index
