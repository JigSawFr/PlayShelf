// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/nlib_flow.hpp"

#include <borealis.hpp>
#include <borealis/cadran/ui/live_prefs.hpp>
#include <ctime>
#include <mutex>
#include <set>
#include <sys/stat.h>

#include <fmt/format.h>

#include "library/catalog.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

namespace nlib_flow
{
namespace
{
constexpr time_t KEEP = 7 * 24 * 3600;   // a game's details, kept this long

std::mutex         s_lock;
std::set<uint64_t> s_unknown;   // the games Nlib API did not know (s_lock)

// cache_dir()/nlib/<title id>-<language>.json
std::string file_of(uint64_t base, const std::string& language)
{
    return fmt::format("{}/nlib/{:016X}-{}.json", paths::cache_dir(), base, language);
}

bool fresh(const std::string& file)
{
    struct stat st;
    return ::stat(file.c_str(), &st) == 0 && std::time(nullptr) - st.st_mtime < KEEP;
}

// On the worker: the details, from the cache while it is fresh, else from
// Nlib API (kept when they read); nullptr when none came.
std::shared_ptr<const titledb::Details> fetch(uint64_t base, const std::string& language)
{
    const std::string file = file_of(base, language);
    auto              d = std::make_shared<titledb::Details>();
    std::string       text;
    if (fresh(file) && paths::read_file(file, text) && titledb::parse_nlib_details(text, base, d.get())) return d;
    http::Response r;
    std::string    why;
    if (!http::request(titledb::nlib_details_url(base, language), {}, "", &r, &why, titledb::NLIB_MAX_DETAILS, 10) ||
        r.status != 200 || !titledb::parse_nlib_details(r.body, base, d.get())) {
        brls::Logger::debug("nlib: no details for {:016X} ({} {})", base, r.status, why);
        // A game it does not know; a failure of the network is tried again.
        if (r.status == 404) {
            std::lock_guard<std::mutex> hold(s_lock);
            s_unknown.insert(base);
        }
        return nullptr;
    }
    if (paths::ensure_dir(paths::cache_dir()) && paths::ensure_dir(paths::cache_dir() + "/nlib")) paths::atomic_write(file, r.body);
    return d;
}
}   // namespace

void details(uint64_t title_id, std::function<void(std::shared_ptr<const titledb::Details>)> done)
{
    const uint64_t base = library::base_of(title_id);
    if (!config::get().nlib_api || titledb::nlib_details_url(base, "en").empty()) return done(nullptr);
    {
        std::lock_guard<std::mutex> hold(s_lock);
        if (s_unknown.count(base)) return done(nullptr);
    }
    const std::string language = titledb::nlib_language(live_prefs::locale());
    ui::in_background("nlib details", [base, language, done]() {
        auto d = fetch(base, language);
        brls::sync([d, done]() { done(d); });
    });
}

}   // namespace nlib_flow
