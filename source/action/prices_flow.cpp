// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/prices_flow.hpp"

#include <map>

#include <aurore.hpp>
#include <aurore/cadran/ui/live_prefs.hpp>

#include "action/insights_index.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

namespace prices_flow
{

namespace
{
// The Switch's prices, in PlayInsights' switch/prices/ folder.
constexpr const char* FOLDER = "switch/prices/";
constexpr const char* BASE = "https://cdn.jsdelivr.net/gh/JigSawFr/PlayInsights@data/";
constexpr size_t      MAX_BYTES = 4 * 1024 * 1024;
constexpr long        KEEP = 24 * 3600;       // fetched again a day after
constexpr long        STALE = 4 * 24 * 3600;  // a cached file older is not shown

std::shared_ptr<const library::PriceBook> s_book = std::make_shared<library::PriceBook>();
std::string                               s_country;
int                                       s_run = 0;   // the latest start(): an older one's answer is dropped
std::map<int, std::function<void()>>      s_listeners;
int                                       s_next = 1;

void tell()
{
    const auto listeners = s_listeners;
    for (const auto& l : listeners) l.second();
}

std::string cached(const std::string& country)
{
    return paths::cache_dir() + "/prices_" + country + ".json";
}

std::shared_ptr<const library::PriceBook> parse(const std::string& text, const std::string& country)
{
    auto book = std::make_shared<library::PriceBook>();
    return book->from_json(text) && book->country() == country ? book : nullptr;
}

// On the UI thread: `book` shown, when it is the latest start's.
void offer(int run, std::shared_ptr<const library::PriceBook> book)
{
    if (run != s_run || !book) return;
    s_book = std::move(book);
    tell();
}

// The country config says, else that of PlayShelf's language.
std::string chosen()
{
    const std::string& c = config::get().prices_country;
    if (c == "none") return "";
    if (!c.empty()) return c;
    return library::price_country_of(live_prefs::locale());
}
}   // namespace

void start()
{
    const int run = ++s_run;
    s_country = chosen();
    const bool had = s_book->size() > 0;
    s_book = std::make_shared<library::PriceBook>();
    if (had) tell();
    if (s_country.empty()) return;
    const std::string country = s_country;
    const bool        online = config::get().popular_online;
    ui::in_background("prices", [run, country, online]() {
        const std::string file = cached(country);
        const long        age = insights_index::age(file);
        std::string       text;
        const bool        shown = age >= 0 && age < STALE && paths::read_file(file, text);
        if (shown) {
            auto book = parse(text, country);
            aurore::sync([run, book]() { offer(run, book); });
        }
        if (!online || (age >= 0 && age < KEEP)) return;
        // Unchanged since (the index says): kept, and shown when it was too old to be.
        if (insights_index::unchanged(std::string(FOLDER) + country + ".json", file)) {
            if (!shown && paths::read_file(file, text)) {
                auto book = parse(text, country);
                aurore::sync([run, book]() { offer(run, book); });
            }
            return;
        }
        http::Response r;
        std::string    why;
        if (!http::request(std::string(BASE) + FOLDER + country + ".json", {}, "", &r, &why, MAX_BYTES, 30) || r.status != 200) {
            aurore::Logger::debug("prices: {} not fetched ({} {})", country, r.status, why);
            return;
        }
        auto book = parse(r.body, country);
        if (!book) {
            aurore::Logger::warning("prices: the fetched {} is not one", country);
            return;
        }
        if (paths::ensure_dir(paths::cache_dir())) paths::atomic_write(file, r.body);
        aurore::Logger::info("prices: {}, {} games, made {}", country, book->size(), book->generated());
        aurore::sync([run, book]() { offer(run, book); });
    });
}

std::string country()
{
    return s_country;
}

std::shared_ptr<const library::PriceBook> book()
{
    return s_book;
}

const library::Price* of(uint64_t base_id)
{
    return s_book->find(base_id);
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

}   // namespace prices_flow
