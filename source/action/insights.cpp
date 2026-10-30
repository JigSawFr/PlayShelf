// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/insights.hpp"

#include <algorithm>
#include <ctime>
#include <format>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

#include <aurore.hpp>

#include "action/changes_flow.hpp"
#include "action/download_flow.hpp"
#include "action/nlib_flow.hpp"
#include "action/popular_flow.hpp"
#include "action/prices_flow.hpp"
#include "action/titledb_flow.hpp"
#include "action/not_interested.hpp"
#include "action/wishlist.hpp"
#include "library/installed.hpp"
#include "library/seen.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"
#include "util/listeners.hpp"
#include "util/paths.hpp"
#include "util/title_id.hpp"

using namespace aurore::literals;

namespace insights
{

namespace
{
constexpr size_t   ROW = 12;              // games in a row
constexpr uint32_t ADDED_DAYS = 30;       // "new on your servers": first seen in the last month
constexpr uint64_t IDLE_S = 90 * 86400;   // a game not played for three months
constexpr uint64_t IDLE_SIZE = 1000000000;   // and taking 1 GB or more
constexpr int      TRENDING = 25;         // read a quarter more than before, at least
constexpr int      POPULAR = 20;          // a score worth showing

std::shared_ptr<const Snapshot>      s_now = std::make_shared<Snapshot>();
Listeners                            s_listeners;
bool                                 s_started = false;
bool                                 s_pending = false;   // a make() asked for the next frame

// One cover a game in a row (editions.json): the game itself when the row
// has it, else the first of its editions.
template <typename T, typename IdOf>
void one_per_game(std::vector<T>& row, const library::EditionBook& editions, IdOf id_of)
{
    std::set<uint64_t> games;
    for (const T& x : row)
        if (editions.game_of(id_of(x)) == id_of(x)) games.insert(id_of(x));
    std::set<uint64_t> kept;
    std::vector<T>     out;
    for (T& x : row) {
        const uint64_t id = id_of(x), game = editions.game_of(id);
        if ((id != game && games.contains(game)) || !kept.insert(game).second) continue;
        out.push_back(std::move(x));
    }
    row = std::move(out);
}
std::set<std::string>                s_tried;             // the servers whose catalogue was asked in this run
std::set<std::string>                s_noted;             // and noted in the seen book
std::string                          s_jobs_key;          // the downloads, as last made
library::SeenBook                    s_seen;
bool                                 s_seen_read = false;
// seen.json is a newer build's: the book kept in memory only.
bool                                 s_seen_newer = false;

std::string seen_file()
{
    return paths::data_dir() + "/seen.json";
}

// Today, the console's clock, as YYYYMMDD; and `days` from it.
uint32_t day_of(std::time_t t)
{
    std::tm local{};
    localtime_r(&t, &local);
    return (uint32_t)((local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday);
}

uint32_t today(long days = 0)
{
    return day_of(std::time(nullptr) + days * 86400);
}

uint32_t date_of(const library::Title& t)
{
    const uint32_t known = nlib_flow::facts(t.id).release_date;
    return known ? known : t.release_date;
}

// The catalogues read in this run, the servers' order.
std::vector<std::shared_ptr<const library_flow::Loaded>> catalogues()
{
    std::vector<std::shared_ptr<const library_flow::Loaded>> out;
    for (const library::Server& s : library::load_servers())
        if (auto l = library_flow::loaded(s.id)) out.push_back(std::move(l));
    return out;
}

void tell()
{
    s_listeners.notify();
}

// The seen book written, off the UI thread.
void save_seen()
{
    if (s_seen_newer) return;
    ui::save_later("seen", seen_file(), s_seen.to_json());
}

// A catalogue read for the first time in this run: its games noted.
void note_seen(const library_flow::Loaded& l)
{
    if (!s_seen_read || !s_noted.insert(l.server.id).second) return;
    std::vector<uint64_t> ids;
    ids.reserve(l.titles.size());
    for (const library::Title& t : l.titles)
        if (t.id) ids.push_back(t.id);
    if (s_seen.note(l.server.id, ids, today())) save_seen();
}

void make();

// Made again a moment later, once, however many changes come meanwhile
// (Nlib API's facts come a few at a time).
void soon()
{
    if (s_pending) return;
    s_pending = true;
    aurore::delay(300, []() {
        aurore::sync([]() {
            s_pending = false;
            make();
        });
    });
}

// The next server whose catalogue was not asked yet, read.
void read_next()
{
    if (library_flow::loading()) return;
    for (const library::Server& s : library::load_servers()) {
        if (library_flow::loaded(s.id) || s_tried.contains(s.id)) continue;
        s_tried.insert(s.id);
        library_flow::load(s, [](const std::shared_ptr<const library_flow::Loaded>&, const std::string&) {
            soon();
            read_next();
        });
        return;
    }
}

// The newest update of `t`'s catalogue, -1 when it has none.
long newest_update(const library::Title& t)
{
    return t.updates.empty() ? -1 : (long)t.updates.front();
}

void make()
{
    auto        out = std::make_shared<Snapshot>();
    Snapshot&   s = *out;
    const auto  all = catalogues();
    for (const auto& l : all) note_seen(*l);
    const auto  installed = library_flow::installed();
    const auto  games = installed_flow::games();
    s.loading = library_flow::loading();
    s.space = installed_flow::space();
    s.console_games = games ? games->size() : 0;

    // The games of the servers, each once (its first server's), and where
    // each one is, the servers' order.
    std::vector<Pick>                                    every;
    std::unordered_map<uint64_t, std::vector<Pick>>      where;
    for (const auto& l : all)
        for (size_t i = 0; i < l->titles.size(); i++) {
            const library::Title& t = l->titles[i];
            if (!t.id) continue;
            std::vector<Pick>& at = where[t.id];
            if (at.empty()) every.push_back({ l, i });
            at.push_back({ l, i });
        }
    s.server_games = every.size();

    // A game's catalogue, the first server's that has it.
    auto found_in = [&where](uint64_t id) {
        const auto it = where.find(id);
        return it == where.end() ? Pick{} : it->second.front();
    };

    // The console's games: what they lack, how much they were played; the
    // genres they have, each weighed by how many have it and how much they
    // were played.
    std::vector<std::pair<uint64_t, Pick>> getting;
    std::map<std::string, double>          genres;
    std::vector<library::Owned>            owned;
    const uint64_t                         now = (uint64_t)std::time(nullptr);
    if (games)
        for (const installed_flow::Game& g : *games) {
            const double weight = library::owned_weight(g.play_s, g.launches, g.last_played, now);
            owned.push_back({ g.game.id, weight, (uint32_t)(g.play_s / 3600) });
            for (const std::string& c : nlib_flow::facts(g.game.id).categories)
                if (c != "Other") genres[c] += weight;
            if (!g.launches && !g.play_s) s.never_played++;
            const Pick found = found_in(g.game.id);
            uint32_t   served = 0;
            if (found.data) {
                const library::TitleState st = library::state_of(found.title(), found.data->fetch.catalog, *installed);
                s.updates += st.update_available ? 1 : 0;
                s.dlcs += st.dlcs_missing;
                if (st.update_available || st.dlcs_missing) getting.push_back({ g.play_s, found });
            }
            // The newest update any server has, against the databases' newest.
            if (const auto it = where.find(g.game.id); it != where.end())
                for (const Pick& p : it->second) {
                    const long u = newest_update(p.title());
                    if (u >= 0) served = std::max(served, p.data->fetch.catalog.items[(size_t)u].version);
                }
            if (const auto known = titledb_flow::title(g.game.id | 0x800))
                if (known->version && *known->version > std::max(served, g.game.update) && !all.empty())
                    s.missing.push_back({ g.game.id, name_of(g.game.id, g.name), *known->version, served });
            // Not played for months, and large.
            if (g.size >= IDLE_SIZE && (!g.last_played || now - g.last_played > IDLE_S))
                s.idle.push_back({ g, name_of(g.game.id, g.name), found });
            if (g.last_played) s.recent.push_back({ g, name_of(g.game.id, g.name), found });
        }
    std::stable_sort(getting.begin(), getting.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (const auto& g : getting) s.to_get.push_back(g.second);
    s.to_get_games = s.to_get.size();
    std::stable_sort(s.recent.begin(), s.recent.end(),
                     [](const Played& a, const Played& b) { return a.game.last_played > b.game.last_played; });
    if (s.recent.size() > ROW) s.recent.resize(ROW);
    std::stable_sort(s.idle.begin(), s.idle.end(), [](const Played& a, const Played& b) { return a.game.size > b.game.size; });

    // The games the player put away (not_interested), and those the console
    // has another edition of (editions.json): never suggested, so out of the
    // rows that pick for them.
    const std::vector<uint64_t> unwanted = not_interested::games();
    const auto                  editions = popular_flow::editions();
    std::set<uint64_t>          owned_games;
    for (const library::Owned& o : owned) owned_games.insert(editions->game_of(o.id));
    auto wanted = [&unwanted, &editions, &owned_games](const Pick& p) {
        const uint64_t id = library::base_of(p.title().id);
        return !std::ranges::contains(unwanted, id) && !owned_games.contains(editions->game_of(id));
    };

    // The servers' games the console has not (no demos): by date, by when
    // they came, by how well known they are.
    const uint32_t        month_ago = today(-31), to_day = today();
    std::vector<Pick>     fresh;
    std::vector<uint32_t> came;
    for (const Pick& p : every) {
        const library::Title& t = p.title();
        if (library::state_of(t, p.data->fetch.catalog, *installed).base || nlib_flow::facts(t.id).demo) continue;
        fresh.push_back(p);
        const uint32_t d = date_of(t);
        if (d) s.newest.push_back(p);
        if (d >= month_ago && d <= to_day) s.month.push_back(p);
        if (!wanted(p)) continue;
        const uint32_t seen = s_seen.first_seen(p.data->server.id, t.id);
        if (seen && library::days_between(seen, to_day) <= (long)ADDED_DAYS) s.added.push_back(p);
        const library::Renown r = popular_flow::of(t.id);
        if (r.score >= POPULAR) s.popular.push_back(p);
        if (r.score >= POPULAR && r.trend >= TRENDING) s.trending.push_back(p);
    }
    auto by_date = [](const Pick& a, const Pick& b) { return date_of(a.title()) > date_of(b.title()); };
    std::stable_sort(s.newest.begin(), s.newest.end(), by_date);
    std::stable_sort(s.month.begin(), s.month.end(), by_date);
    std::stable_sort(s.added.begin(), s.added.end(), [](const Pick& a, const Pick& b) {
        return s_seen.first_seen(a.data->server.id, a.title().id) > s_seen.first_seen(b.data->server.id, b.title().id);
    });
    std::stable_sort(s.popular.begin(), s.popular.end(), [](const Pick& a, const Pick& b) {
        return popular_flow::of(a.title().id).score > popular_flow::of(b.title().id).score;
    });
    std::stable_sort(s.trending.begin(), s.trending.end(), [](const Pick& a, const Pick& b) {
        return popular_flow::of(a.title().id).trend > popular_flow::of(b.title().id).trend;
    });
    for (auto* row : { &s.newest, &s.month, &s.added, &s.popular, &s.trending }) {
        one_per_game(*row, *editions, [](const Pick& p) { return p.title().id; });
        if (row->size() > ROW) row->resize(ROW);
    }

    // For the console's player, from every signal: games like the ones it
    // plays, of their series and developers, of their genres, well known and
    // liked, out lately (library/recommend), each with its reason.
    const auto signals = popular_flow::signals();
    const auto similar = popular_flow::similar();
    {
        std::vector<library::Candidate> candidates;
        std::map<uint64_t, Pick>        by_id;
        for (const Pick& p : fresh) {
            const library::Title& t = p.title();
            library::Candidate    c;
            c.id = t.id;
            c.release_date = date_of(t);
            c.genres = nlib_flow::facts(t.id).categories;
            c.renown = popular_flow::of(t.id);
            candidates.push_back(std::move(c));
            by_id[t.id] = p;
        }
        for (library::Suggestion& sug :
             library::recommend(owned, candidates, genres, *similar, *signals, unwanted, to_day, ROW, *editions))
            s.like.push_back({ by_id[sug.id], std::move(sug.reason) });
    }

    // In the series of the console's games: the most played game's series first.
    {
        std::map<std::string, std::pair<double, std::string>> mine;   // series id: weight, name
        for (const library::Owned& o : owned)
            if (const library::GameSignals* g = signals->find(o.id))
                for (const library::Trait& t : g->series) {
                    auto& m = mine[t.id];
                    if (o.weight > m.first) m = { o.weight, t.label };
                }
        std::vector<std::pair<double, InSeries>> found;
        for (const Pick& p : fresh) {
            const library::GameSignals* g = signals->find(p.title().id);
            if (!g || !wanted(p)) continue;
            for (const library::Trait& t : g->series) {
                const auto it = mine.find(t.id);
                if (it == mine.end() || it->second.second.empty()) continue;
                found.push_back({ it->second.first + popular_flow::of(p.title().id).score / 1e3, { p, it->second.second } });
                break;
            }
        }
        std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        one_per_game(found, *editions, [](const auto& f) { return f.second.pick.title().id; });
        for (auto& f : found) {
            if (s.series.size() >= ROW) break;
            s.series.push_back(std::move(f.second));
        }
    }

    // On sale on the eShop of the player's country: the biggest discount
    // first, then the best known; a game on two servers once.
    {
        std::map<uint64_t, Deal> on_sale;
        for (const Pick& p : fresh) {
            const library::Price* price = prices_flow::of(p.title().id);
            if (price && price->on_sale() && wanted(p) && !on_sale.count(p.title().id)) on_sale[p.title().id] = { p, *price };
        }
        for (auto& d : on_sale) s.deals.push_back(std::move(d.second));
        one_per_game(s.deals, *editions, [](const Deal& d) { return d.pick.title().id; });
        std::stable_sort(s.deals.begin(), s.deals.end(), [](const Deal& a, const Deal& b) {
            if (a.price.discount() != b.price.discount()) return a.price.discount() > b.price.discount();
            return popular_flow::of(a.pick.title().id).score > popular_flow::of(b.pick.title().id).score;
        });
        if (s.deals.size() > ROW) s.deals.resize(ROW);
    }

    // Coming out in the next two months (the title databases), better known
    // first, the console's servers aside.
    for (uint64_t id : titledb_flow::released(today(1), today(62), 400)) {
        if (where.contains(id)) continue;
        const auto t = titledb_flow::title(id);
        if (!t || t->name.empty()) continue;
        s.coming.push_back({ id, t->name, t->publisher, t->release_date });
    }
    std::stable_sort(s.coming.begin(), s.coming.end(), [](const Coming& a, const Coming& b) {
        const int x = popular_flow::of(a.id).score, y = popular_flow::of(b.id).score;
        return x != y ? x > y : a.date < b.date;
    });
    if (s.coming.size() > ROW) s.coming.resize(ROW);
    std::stable_sort(s.coming.begin(), s.coming.end(), [](const Coming& a, const Coming& b) { return a.date < b.date; });

    // The wishlist: those a server has first, the order they were added.
    // One known by its id alone: Nlib API asked for its name.
    std::vector<uint64_t> unnamed;
    for (uint64_t id : wishlist::games()) {
        Wished w;
        w.id = id;
        w.found = found_in(id);
        w.name = w.found.data ? w.found.title().name : name_of(id, "");
        if (!w.found.data && w.name == title_id::text(id)) unnamed.push_back(id);
        w.installed = installed && installed->count(id);
        s.wished.push_back(std::move(w));
    }
    std::stable_partition(s.wished.begin(), s.wished.end(), [](const Wished& w) { return w.found.data && !w.installed; });
    if (!unnamed.empty()) nlib_flow::enrich(unnamed);

    for (const download::Job& j : download_flow::jobs())
        if (j.state != download::State::Done) s.downloads++;

    s_now = out;
    tell();
}
}   // namespace

void start()
{
    if (s_started) return;
    s_started = true;
    {
        std::string text;
        if (paths::read_file(seen_file(), text) && library::SeenBook::newer(text)) {
            aurore::Logger::warning("insights: {} is from a newer PlayShelf, left as it is", seen_file());
            s_seen_newer = true;
        } else if (!text.empty() && !s_seen.from_json(text))
            aurore::Logger::warning("insights: {} not read", seen_file());
        s_seen_read = true;
    }
    // What changes what is made.
    nlib_flow::listen(soon);
    popular_flow::listen(soon);
    prices_flow::listen(soon);
    changes_flow::listen(soon);
    wishlist::listen(soon);
    not_interested::listen(soon);
    titledb_flow::listen(soon);
    download_flow::listen([]() {
        // A running download's speed, for its server's rank.
        const download_flow::Live live = download_flow::live();
        std::string               key;
        for (const download::Job& j : download_flow::jobs()) {
            key += j.id + ":" + std::to_string(std::to_underlying(j.state)) + ";";
            if (j.id == live.id && live.speed > 0) library_flow::note_speed(j.server_id, live.speed);
        }
        // The jobs and their states, not their progress. One more done: the
        // console's games read again.
        if (key == s_jobs_key) return;
        const std::string done_mark = ":" + std::to_string(std::to_underlying(download::State::Done)) + ";";
        const auto        done = [&done_mark](const std::string& k) {
            size_t n = 0;
            for (size_t at = k.find(done_mark); at != std::string::npos; at = k.find(done_mark, at + 1)) n++;
            return n;
        };
        const bool installed_one = done(key) > done(s_jobs_key);
        s_jobs_key = key;
        if (installed_one) installed_flow::read([](const std::shared_ptr<const std::vector<installed_flow::Game>>&) { soon(); });
        else soon();
    });
    installed_flow::read([](const std::shared_ptr<const std::vector<installed_flow::Game>>&) {
        soon();
        read_next();
    });
}

void refresh()
{
    soon();
    read_next();
}

std::shared_ptr<const Snapshot> now()
{
    return s_now;
}

void update_all()
{
    if (ui::refuse_read_only()) return;
    // The newest update of each game that has one to get, the most played
    // game's first (to_get is in that order).
    const auto                                   installed = library_flow::installed();
    std::vector<std::pair<Pick, size_t>>         updates;
    uint64_t                                     bytes = 0;
    for (const Pick& p : s_now->to_get) {
        const library::TitleState st = library::state_of(p.title(), p.data->fetch.catalog, *installed);
        const long                u = newest_update(p.title());
        if (!st.update_available || u < 0) continue;
        updates.push_back({ p, (size_t)u });
        bytes += p.data->fetch.catalog.items[(size_t)u].size;
    }
    if (updates.empty()) {
        ui::notify("playshelf/home/no_updates"_i18n);
        return;
    }
    const std::string body = updates.size() == 1 ? aurore::getStr("playshelf/home/update_all_one", library_flow::size_text(bytes))
                                                 : aurore::getStr("playshelf/home/update_all_body", updates.size(),
                                                                library_flow::size_text(bytes));
    ui::confirm(body, "playshelf/home/update_all"_i18n, [updates]() {
        size_t added = 0;
        for (const auto& [p, item] : updates) added += library_flow::install(*p.data, { item });
        if (added) ui::notify(aurore::getStr("playshelf/downloads/added", added), ui::Toast::Done);
        else ui::notify("playshelf/downloads/already"_i18n);
    });
}

int listen(std::function<void()> changed)
{
    return s_listeners.listen(std::move(changed));
}

void unlisten(int id)
{
    s_listeners.unlisten(id);
}

std::string name_of(uint64_t id, const std::string& console)
{
    if (!console.empty()) return console;
    if (const auto t = titledb_flow::title(id))
        if (!t->name.empty()) return t->name;
    const std::string known = nlib_flow::facts(id).name;
    if (!known.empty()) return known;
    // Not told: the console's name of it, else a catalogue's.
    if (const auto games = installed_flow::games())
        for (const installed_flow::Game& g : *games)
            if (g.game.id == id && !g.name.empty()) return g.name;
    const Pick found = found_of(id);
    if (found.data && !found.title().name.empty()) return found.title().name;
    return title_id::text(id);
}

Pick found_of(uint64_t id)
{
    for (const auto& l : catalogues()) {
        const long t = library::title_of(l->titles, id);
        if (t >= 0) return Pick{ l, (size_t)t };
    }
    return Pick{};
}

uint32_t first_seen(const Pick& pick)
{
    return pick.data ? s_seen.first_seen(pick.data->server.id, pick.title().id) : 0;
}

bool nearly_full(const installed_flow::Space& space)
{
    return space.known && space.total && (space.free * 10 < space.total || space.free < 4000000000ULL);
}

}   // namespace insights
