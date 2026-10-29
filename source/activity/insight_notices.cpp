// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/insight_notices.hpp"

#include <set>
#include <string>
#include <vector>

#include <aurore.hpp>
#include <fmt/format.h>

#include "action/download_flow.hpp"
#include "action/game_page.hpp"
#include "action/library_flow.hpp"
#include "action/notices.hpp"
#include "action/prices_flow.hpp"
#include "activity/storage_activity.hpp"
#include "activity/tabs.hpp"
#include "tab/library_tab.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace insight_notices
{

namespace
{
std::set<std::string> s_wished;   // the wishlist's entries set, to clear those gone
std::set<std::string> s_sales;    // the wishlist's sales said, to clear those over

// "Ember Vale, Turbo Kart, Ink Squad…": the first names of a list.
template <typename List, typename Name>
std::string names(const List& list, Name name)
{
    std::string out;
    size_t      n = 0;
    for (const auto& item : list) {
        if (n == 3) return out + "…";
        out += (n++ ? ", " : "") + name(item);
    }
    return out;
}

// "12 November": a sale's last day, in the language's words.
std::string day_text(uint32_t date)
{
    const unsigned d = date % 100, m = date / 100 % 100;
    if (m < 1 || m > 12) return "";
    return aurore::getStr("playshelf/notices/day", d, aurore::getStr(fmt::format("cadran/months/{}", m)));
}

// `one` for 1, else `many` with the count.
std::string count_text(size_t n, const char* one, const char* many)
{
    return n == 1 ? aurore::getStr(one) : aurore::getStr(many, n);
}

void library_with(library::Filter::State state)
{
    LibraryTab::preset(state);
    ui::open_tab(tabs::library);
}
}   // namespace

void publish(const insights::Snapshot& s)
{
    const std::string see = "playshelf/notices/see"_i18n;

    // The updates to get: all at once, the most played game's first.
    if (s.updates) {
        std::vector<insights::Pick> with;
        const auto installed = library_flow::installed();
        for (const insights::Pick& p : s.to_get)
            if (library::state_of(p.title(), p.data->fetch.catalog, *installed).update_available) with.push_back(p);
        notices::set("updates", notices::Level::Action,
                     count_text(s.updates, "playshelf/notices/updates_one", "playshelf/notices/updates"),
                     names(with, [](const insights::Pick& p) { return p.title().name; }), "playshelf/home/update_all"_i18n,
                     []() { insights::update_all(); });
    } else {
        notices::clear("updates");
    }
    if (s.dlcs)
        notices::set("dlcs", notices::Level::Action, count_text(s.dlcs, "playshelf/notices/dlcs_one", "playshelf/notices/dlcs"), "",
                     see, []() { library_with(library::Filter::State::DlcsMissing); });
    else
        notices::clear("dlcs");

    // The games new on the servers: Home shows them.
    if (!s.added.empty())
        notices::set("new_games", notices::Level::Info,
                     count_text(s.added.size(), "playshelf/notices/new_games_one", "playshelf/notices/new_games"),
                     names(s.added, [](const insights::Pick& p) { return p.title().name; }), see,
                     []() { ui::open_tab(tabs::home); });
    else
        notices::clear("new_games");

    // The wishlist's games a server has, the console not.
    std::set<std::string> wished;
    for (const insights::Wished& w : s.wished) {
        if (!w.found.data || w.installed) continue;
        const std::string id = fmt::format("wish.{:016x}", w.id);
        wished.insert(id);
        const auto   data = w.found.data;
        const size_t index = w.found.index;
        notices::set(id, notices::Level::Action, aurore::getStr("playshelf/notices/wished", w.name, data->server.name),
                     "playshelf/notices/wished_detail"_i18n, see, [data, index]() { game_page::open(data, index); });
    }
    for (const std::string& id : s_wished)
        if (!wished.contains(id)) notices::clear(id);
    s_wished = wished;

    // The wishlist's games on sale on the eShop of the player's country: for
    // information, once a sale (its id has the sale's last day: put away, it
    // comes back with the next sale).
    std::set<std::string> sales;
    const std::string     country = prices_flow::country();
    for (const insights::Wished& w : s.wished) {
        const library::Price* price = prices_flow::of(w.id);
        if (w.installed || !price || !price->on_sale()) continue;
        const std::string id = fmt::format("sale.{:016x}.{}", w.id, price->until);
        sales.insert(id);
        const std::string sale = library::price_text(price->sale, country);
        const std::string until = price->until ? day_text(price->until) : "";
        const auto        data = w.found.data;
        const size_t      index = w.found.index;
        std::function<void()> open;
        if (data) open = [data, index]() { game_page::open(data, index); };
        notices::set(id, notices::Level::Info,
                     aurore::getStr(price->at_lowest() ? "playshelf/notices/sale_lowest" : "playshelf/notices/sale", w.name),
                     until.empty() ? aurore::getStr("playshelf/home/deal", price->discount(), sale)
                                   : aurore::getStr("playshelf/notices/sale_until", price->discount(), sale, until),
                     data ? see : "", open);
    }
    for (const std::string& id : s_sales)
        if (!sales.count(id)) notices::clear(id);
    s_sales = sales;

    // The updates no server has yet: for information.
    if (!s.missing.empty())
        notices::set("missing_updates", notices::Level::Info,
                     count_text(s.missing.size(), "playshelf/notices/missing_one", "playshelf/notices/missing"),
                     names(s.missing, [](const insights::Missing& m) { return m.name; }));
    else
        notices::clear("missing_updates");

    // The SD card nearly full.
    if (insights::nearly_full(s.space))
        notices::set("storage", notices::Level::Action, "playshelf/notices/storage"_i18n,
                     aurore::getStr("playshelf/storage/free_of", library_flow::size_text(s.space.free),
                                  library_flow::size_text(s.space.total)),
                     "playshelf/notices/make_room"_i18n, []() { StorageActivity::open(); });
    else
        notices::clear("storage");

    // The downloads: installed (until the list is cleared), failed.
    size_t done = 0, failed = 0;
    for (const download::Job& j : download_flow::jobs()) {
        done += j.state == download::State::Done;
        failed += j.state == download::State::Failed;
    }
    const auto downloads = []() { ui::open_tab(tabs::downloads); };
    if (done)
        notices::set("downloads_done", notices::Level::Info,
                     count_text(done, "playshelf/notices/downloads_done_one", "playshelf/notices/downloads_done"), "", see,
                     downloads);
    else
        notices::clear("downloads_done");
    if (failed)
        notices::set("downloads_failed", notices::Level::Action,
                     count_text(failed, "playshelf/notices/downloads_failed_one", "playshelf/notices/downloads_failed"), "",
                     see, downloads);
    else
        notices::clear("downloads_failed");
}

}   // namespace insight_notices
