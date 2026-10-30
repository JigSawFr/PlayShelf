// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/insight_notices.hpp"

#include <format>
#include <set>
#include <string>
#include <vector>

#include <aurore.hpp>

#include "action/changes_flow.hpp"
#include "action/download_flow.hpp"
#include "action/game_page.hpp"
#include "action/library_flow.hpp"
#include "action/notices.hpp"
#include "action/popular_flow.hpp"
#include "activity/storage_activity.hpp"
#include "activity/tabs.hpp"
#include "library/news.hpp"
#include "util/paths.hpp"
#include "tab/library_tab.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace insight_notices
{

namespace
{
std::set<std::string> s_wished;   // the wishlist's entries set, to clear those gone
std::set<std::string> s_news;     // the last run's news said (out today), to clear the older
std::set<std::string> s_atlas;    // the last run's new games and trends said, to clear the older

// The games told new or trending once (library/news), read at the first
// publish; news.json of a newer build is kept in memory only.
library::TellBook s_told;
bool              s_told_read = false, s_told_newer = false;

std::string told_file() { return paths::data_dir() + "/news.json"; }

void read_told()
{
    if (s_told_read) return;
    s_told_read = true;
    std::string text;
    if (!paths::read_file(told_file(), text) || text.empty()) return;
    if (library::TellBook::newer(text)) {
        aurore::Logger::warning("notices: {} is from a newer PlayShelf, left as it is", told_file());
        s_told_newer = true;
    } else if (!s_told.from_json(text)) {
        aurore::Logger::warning("notices: {} not read", told_file());
    }
}

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

// "12 November": a day of Atlas' run, in the language's words.
std::string day_text(uint32_t date)
{
    const unsigned d = date % 100, m = date / 100 % 100;
    if (m < 1 || m > 12) return "";
    return aurore::getStr("playshelf/notices/day", d, aurore::getStr(std::format("cadran/months/{}", m)));
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
        const std::string id = std::format("wish.{:016x}", w.id);
        wished.insert(id);
        const auto   data = w.found.data;
        const size_t index = w.found.index;
        notices::set(id, notices::Level::Action, aurore::getStr("playshelf/notices/wished", w.name, data->server.name),
                     "playshelf/notices/wished_detail"_i18n, see, [data, index]() { game_page::open(data, index); });
    }
    for (const std::string& id : s_wished)
        if (!wished.contains(id)) notices::clear(id);
    s_wished = wished;

    const auto changes = changes_flow::book();
    // From the last run of Atlas (changes.json), for the wishlist's games
    // the console has not: one out that day, as long as that run is the last
    // one known: the next one's news replaces it (its id has the day: put away, it
    // does not come back for the same news).
    std::set<std::string> news;
    const uint32_t        run_day = changes->day_number();
    for (const insights::Wished& w : s.wished) {
        if (w.installed || !run_day) continue;
        const auto            data = w.found.data;
        const size_t          index = w.found.index;
        std::function<void()> open;
        if (data) open = [data, index]() { game_page::open(data, index); };
        if (changes->out_today(w.id)) {
            const std::string id = std::format("out.{:016x}.{}", w.id, run_day);
            news.insert(id);
            notices::set(id, notices::Level::Info, aurore::getStr("playshelf/notices/out", w.name),
                         aurore::getStr("playshelf/notices/out_on", day_text(run_day)), data ? see : "", open);
        }
    }
    for (const std::string& id : s_news)
        if (!news.contains(id)) notices::clear(id);
    s_news = news;

    // Beyond the wishlist (library/news): the games new on the Switch of a
    // series the console's or the wishlist's games are of, or much like one
    // of them; the wishlist's games, and those of the console's series, read
    // much more lately. Each game told once, while its run is the last one.
    std::set<std::string> atlas;
    if (run_day) {
        read_told();
        std::vector<uint64_t> wished_ids;
        for (const insights::Wished& w : s.wished) wished_ids.push_back(w.id);
        bool changed = s_told.forget_older(run_day);
        for (const library::News& n : library::news_of(*changes, *popular_flow::signals(), *popular_flow::similar(),
                                                       *popular_flow::editions(), s.owned, wished_ids, s_told)) {
            const bool        fresh = n.kind == library::NewsKind::New;
            const std::string id = std::format("{}.{:016x}.{}", fresh ? "atlas_new" : "trend", n.id, run_day);
            atlas.insert(id);
            changed |= s_told.note(n.kind, n.id, run_day);
            const std::string     name = insights::name_of(n.id);
            const insights::Pick  found = insights::found_of(n.id);
            std::function<void()> open;
            if (found.data) open = [data = found.data, index = found.index]() { game_page::open(data, index); };
            std::string title, detail;
            if (fresh) {
                title = aurore::getStr("playshelf/notices/atlas_new", name);
                detail = n.why == library::News::Why::Alike ? aurore::getStr("playshelf/notices/atlas_new_like", insights::name_of(n.like))
                                                            : aurore::getStr("playshelf/notices/atlas_new_series", n.series);
            } else {
                title = aurore::getStr("playshelf/notices/trend", name);
                detail = n.why == library::News::Why::Wished ? aurore::getStr("playshelf/notices/trend_detail", n.now)
                                                             : aurore::getStr("playshelf/notices/trend_series", n.series, n.now);
            }
            notices::set(id, notices::Level::Info, title, detail, found.data ? see : "", open);
        }
        if (changed && !s_told_newer) ui::save_later("news", told_file(), s_told.to_json());
    }
    for (const std::string& id : s_atlas)
        if (!atlas.contains(id)) notices::clear(id);
    s_atlas = atlas;

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
