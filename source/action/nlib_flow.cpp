// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/nlib_flow.hpp"

#include <algorithm>
#include <format>
#include <aurore.hpp>
#include <aurore/cadran/ui/live_prefs.hpp>
#include <ctime>
#include <deque>
#include <mutex>
#include <set>
#include <sys/stat.h>

#include "action/titledb_flow.hpp"
#include "library/catalog.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/listeners.hpp"
#include "util/paths.hpp"

namespace nlib_flow
{
namespace
{
constexpr time_t KEEP = 7 * 24 * 3600;   // a game's details, kept this long
constexpr size_t TELL_EVERY = 8;         // facts told to the listeners a few at a time

std::mutex         s_lock;
std::set<uint64_t> s_unknown;   // the games Nlib API did not know (s_lock)
std::deque<uint64_t> s_queue;   // enrich()'s games, not asked yet (s_lock)
bool               s_running = false;   // enrich()'s worker (s_lock)
std::string        s_language;          // its language (s_lock)

// On the UI thread.
library::FactsBook                    s_book;
bool                                  s_dirty = false;   // facts not written yet
size_t                                s_untold = 0;      // facts the listeners were not told of
Listeners                            s_listeners;

std::string dir()
{
    return paths::data_dir() + "/nlib";
}

std::string book_file()
{
    return dir() + "/facts.json";
}

// cache_dir()/nlib/<title id>-<language>-2.json (2: the fields asked since
// the facts are kept).
std::string file_of(uint64_t base, const std::string& language)
{
    return std::format("{}/nlib/{:016X}-{}-2.json", paths::cache_dir(), base, language);
}

bool fresh(const std::string& file)
{
    struct stat st;
    return ::stat(file.c_str(), &st) == 0 && std::time(nullptr) - st.st_mtime < KEEP;
}

bool is_unknown(uint64_t base)
{
    std::lock_guard<std::mutex> hold(s_lock);
    return s_unknown.contains(base);
}

// On the worker: the details, from the cache while it is fresh, else from
// Nlib API (kept when they read); nullptr when none came. `offline`: set
// when the network failed (not a game it does not know).
std::shared_ptr<const titledb::Details> fetch(uint64_t base, const std::string& language, bool* offline = nullptr)
{
    const std::string file = file_of(base, language);
    auto              d = std::make_shared<titledb::Details>();
    std::string       text;
    if (fresh(file) && paths::read_file(file, text) && titledb::parse_nlib_details(text, base, d.get())) return d;
    http::Response r;
    std::string    why;
    if (!http::request(titledb::nlib_details_url(base, language), {}, "", &r, &why, titledb::NLIB_MAX_DETAILS, 10) ||
        r.status != 200 || !titledb::parse_nlib_details(r.body, base, d.get())) {
        aurore::Logger::debug("nlib: no details for {:016X} ({} {})", base, r.status, why);
        // A game it does not know; a failure of the network is tried again.
        if (r.status == 404) {
            std::lock_guard<std::mutex> hold(s_lock);
            s_unknown.insert(base);
        } else if (offline) {
            *offline = true;
        }
        return nullptr;
    }
    // A cache: a copy not written is fetched again.
    if (paths::ensure_dir(paths::cache_dir()) && paths::ensure_dir(paths::cache_dir() + "/nlib")) (void)paths::atomic_write(file, r.body);
    return d;
}

void tell()
{
    s_untold = 0;
    s_listeners.notify();
}

// The facts written, off the UI thread.
void save()
{
    if (!s_dirty) return;
    s_dirty = false;
    auto text = std::make_shared<std::string>(s_book.to_json());
    ui::in_background("nlib facts", [text]() {
        // Saved again at the next change: a failed write is not reported.
        if (paths::ensure_dir(dir())) (void)paths::atomic_write(book_file(), *text);
    });
}

// On the UI thread: a game's details kept as its facts.
void keep(uint64_t base, const titledb::Details& d)
{
    library::Facts f;
    f.name = d.name;
    f.publisher = d.publisher;
    f.developer = d.developer;
    f.release_date = library::date_of(d.release_date);
    f.categories = d.categories;
    f.languages = d.languages;
    f.rating = d.rating;
    f.players = d.players;
    f.demo = d.demo;
    f.nsu_id = d.nsu_id;
    if (!s_book.put(base, std::move(f))) return;
    s_dirty = true;
    if (++s_untold >= TELL_EVERY) tell();
}

// enrich()'s worker: one game after the other, until none is left or the
// network fails.
void drain()
{
    for (;;) {
        uint64_t    base = 0;
        std::string language;
        {
            std::lock_guard<std::mutex> hold(s_lock);
            if (s_queue.empty() || ui::quitting()) {
                s_running = false;
                break;
            }
            base = s_queue.front();
            s_queue.pop_front();
            language = s_language;
            if (s_unknown.contains(base)) continue;
        }
        bool offline = false;
        auto d = fetch(base, language, &offline);
        if (d) aurore::sync([base, d]() { keep(base, *d); });
        if (offline) {
            std::lock_guard<std::mutex> hold(s_lock);
            s_queue.clear();
        }
    }
    // Done: the listeners told of the rest, the facts written.
    aurore::sync([]() {
        if (s_untold) tell();
        save();
    });
}
}   // namespace

void start()
{
    ui::in_background("nlib facts", []() {
        auto        book = std::make_shared<library::FactsBook>();
        std::string text;
        if (paths::read_file(book_file(), text) && !book->from_json(text)) aurore::Logger::warning("nlib: {} not read", book_file());
        aurore::sync([book]() {
            // Facts that came meanwhile stay: they are newer.
            s_book.add_missing(*book);
            tell();
        });
    });
}

void details(uint64_t title_id, std::function<void(std::shared_ptr<const titledb::Details>)> done)
{
    const uint64_t base = library::base_of(title_id);
    if (!config::get().nlib_api || titledb::nlib_details_url(base, "en").empty() || is_unknown(base)) return done(nullptr);
    const std::string language = titledb::nlib_language(live_prefs::locale());
    ui::in_background("nlib details", [base, language, done]() {
        auto d = fetch(base, language);
        aurore::sync([base, d, done]() {
            if (d) {
                keep(base, *d);
                if (s_untold) tell();
                save();
            }
            done(d);
        });
    });
}

const library::FactsBook& book()
{
    return s_book;
}

library::Facts facts(uint64_t title_id)
{
    const uint64_t base = library::base_of(title_id);
    if (const library::Facts* f = s_book.find(base)) return *f;
    library::Facts f;
    if (const auto t = titledb_flow::title(base)) {
        f.publisher = t->publisher;
        f.release_date = t->release_date;
    }
    return f;
}

void enrich(const std::vector<uint64_t>& title_ids)
{
    if (!config::get().nlib_api) return;
    std::vector<uint64_t> wanted;
    for (uint64_t id : title_ids) {
        const uint64_t base = library::base_of(id);
        if (titledb::nlib_details_url(base, "en").empty() || s_book.find(base)) continue;
        if (!std::ranges::contains(wanted, base)) wanted.push_back(base);
    }
    bool start_worker = false;
    {
        std::lock_guard<std::mutex> hold(s_lock);
        s_queue.assign(wanted.begin(), wanted.end());
        s_language = titledb::nlib_language(live_prefs::locale());
        if (!s_queue.empty() && !s_running) start_worker = s_running = true;
    }
    if (start_worker) ui::in_background("nlib facts", drain);
}

std::string genre_name(const std::string& genre)
{
    std::string slug;
    for (char c : genre)
        slug += (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) ? c : '_';
    const std::string key = "playshelf/genres/" + slug;
    const std::string s = aurore::getStr(key);
    return s.empty() || s == key ? genre : s;
}

bool waiting(uint64_t title_id)
{
    const uint64_t base = library::base_of(title_id);
    return config::get().nlib_api && !titledb::nlib_details_url(base, "en").empty() && !s_book.find(base) && !is_unknown(base);
}

int listen(std::function<void()> changed)
{
    return s_listeners.listen(std::move(changed));
}

void unlisten(int id)
{
    s_listeners.unlisten(id);
}

}   // namespace nlib_flow
