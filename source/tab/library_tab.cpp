// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/library_tab.hpp"

#include <algorithm>

#include <fmt/format.h>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/cadran_list.hpp>
#include <aurore/cadran/ui/guarded.hpp>

#include "action/download_flow.hpp"
#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "action/services_flow.hpp"
#include "action/titledb_flow.hpp"
#include "activity/game_activity.hpp"
#include "library/servers.hpp"
#include "library/titles.hpp"
#include "tab/filter_sheet.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/title_id.hpp"
#include "view/game_cell.hpp"
#include "view/game_tile.hpp"
#include "view/tool_chip.hpp"

using namespace aurore::literals;

namespace
{
constexpr size_t PAGE = 40;   // covers built at a time

// The tab's room: the screen less the rail and the column's sides.
float room()
{
    return aurore::Application::contentWidth - aurore::Application::getStyle()["aurore/tab_frame/sidebar_width"] -
           2 * aurore::Application::getStyle()["aurore/tab_frame/content_padding_sides"];
}

int columns()
{
    return std::max(1, (int)(room() / GameTile::WIDTH));
}

size_t installed_jobs()
{
    const auto& jobs = download_flow::jobs();
    return (size_t)std::count_if(jobs.begin(), jobs.end(), [](const download::Job& j) { return j.state == download::State::Done; });
}

// The catalogues read this run, the servers' order.
std::vector<std::shared_ptr<const library_flow::Loaded>> catalogues()
{
    std::vector<std::shared_ptr<const library_flow::Loaded>> out;
    for (const library::Server& s : library::load_servers())
        if (auto l = library_flow::loaded(s.id)) out.push_back(std::move(l));
    return out;
}

// The version a game shows: the console's words ("1.2.0"), else its number.
std::string version_text(const installed_flow::Game& g)
{
    if (!g.version.empty()) return aurore::getStr("playshelf/installed/version", g.version);
    return aurore::getStr("playshelf/installed/version", library::running_version(g.game));
}

// What a catalogue has of the game that the console has not.
library::TitleState state_in(const std::shared_ptr<const library_flow::Loaded>& data, long title)
{
    if (!data || title < 0) return {};
    return library::state_of(data->titles[(size_t)title], data->fetch.catalog, *library_flow::installed());
}

bool something_to_get(const library::TitleState& st)
{
    return st.update_available || st.dlcs_missing;
}

// The order and the filter chosen: they stay for this run.
enum class Order : int { Name, Publisher, Newest, ToGetFirst, MostPlayed, RecentlyPlayed };
const char* const ORDERS[] = { "name", "publisher", "newest", "to_get_first", "most_played", "recently_played" };
Order           s_order = Order::Name;
library::Filter s_filter;

bool needs_facts()
{
    const library::Filter& f = s_filter;
    return !f.genre.empty() || !f.publisher.empty() || !f.language.empty() || f.players || f.hide_demos ||
           s_order == Order::Publisher || s_order == Order::Newest;
}
}   // namespace

LibraryTab::LibraryTab()
    : TabBase("xml/tab/library.xml")
{
    cadran::kit::Empty empty = cadran::kit::empty("shelf");
    empty.title->setText("playshelf/installed/empty_title"_i18n);
    empty.text->setText("playshelf/installed/empty_text"_i18n);
    empty_box->addView(empty.box);
    ui::set_visible(empty_box, false);
    // The controls, a pill each.
    summary = new ToolChip("gamepad", "playshelf/installed/summary"_i18n);
    filter_cell = new ToolChip("tools", "playshelf/filters/row_sort"_i18n);
    filter_cell->setVisibility(aurore::Visibility::GONE);
    bar->addView(summary);
    bar->addView(filter_cell);
    cadran::kit::wrap(bar);
    summary->registerClickAction([this](aurore::View*) {
        this->refresh();
        return true;
    });
    filter_cell->registerClickAction([this](aurore::View*) {
        this->pick_filter();
        return true;
    });
    more->registerClickAction([this](aurore::View*) {
        this->add_page();
        return true;
    });
    this->set_hints();
    // A tab's margins, as the other tabs' lists have them.
    const cadran::List margins(this);
    (void)margins;
    // A download installed: what the console has, read again.
    this->done_jobs = installed_jobs();
    this->listener = download_flow::listen(ui::guarded(this->alive, [this]() {
        const size_t n = installed_jobs();
        if (n == this->done_jobs || !this->shown()) return;
        this->done_jobs = n;
        this->refresh();
    }));
    // The title database opened or updated: its names.
    this->titledb_opened = titledb_flow::status().opened;
    this->titledb_listener = titledb_flow::listen(ui::guarded(this->alive, [this]() {
        const int opened = titledb_flow::status().opened;
        if (opened == this->titledb_opened) return;
        this->titledb_opened = opened;
        if (this->shown_games) this->show_games(this->shown_games);
    }));
    // Nlib API's facts came: the names of the games known only by their
    // title id, once one of them has one.
    this->nlib_listener = nlib_flow::listen(ui::guarded(this->alive, [this]() {
        if (!this->shown_games) return;
        for (const Entry& e : this->entries)
            if (e.name == title_id::text(e.game->game.id) && !nlib_flow::facts(e.game->game.id).name.empty())
                return this->show_games(this->shown_games);
        // A filter or the order that needs the facts: the games again.
        if (needs_facts()) this->show_games(this->shown_games);
    }));
}

LibraryTab::~LibraryTab()
{
    download_flow::unlisten(this->listener);
    titledb_flow::unlisten(this->titledb_listener);
    nlib_flow::unlisten(this->nlib_listener);
}

void LibraryTab::preset(library::Filter::State state)
{
    s_filter = library::Filter();
    s_filter.state = state;
}

aurore::View* LibraryTab::create()
{
    return new LibraryTab();
}

void LibraryTab::refresh()
{
    download_flow::show_pill();
    // What the last read had, at once; this read's when it ends.
    if (const auto known = installed_flow::games(); known && known != this->shown_games) this->show_games(known);
    else if (!this->shown_games) summary->set_value("playshelf/installed/reading"_i18n);
    // A catalogue to compare the games with: the first server's when none
    // is read yet.
    const std::vector<library::Server> servers = library::load_servers();
    if (!servers.empty() && catalogues().empty() && !library_flow::loading())
        library_flow::load(servers.front(), ui::guarded(this->alive, [this](const std::shared_ptr<const library_flow::Loaded>& l, const std::string&) {
            if (l && this->shown_games) this->show_games(this->shown_games);
        }));
    installed_flow::read(ui::guarded(this->alive, [this](const std::shared_ptr<const std::vector<installed_flow::Game>>& games) {
        this->show_games(games);
    }));
}

void LibraryTab::show_games(std::shared_ptr<const std::vector<installed_flow::Game>> games)
{
    this->shown_games = std::move(games);
    const auto all = catalogues();
    this->entries.clear();
    size_t to_get = 0;
    for (const installed_flow::Game& g : *this->shown_games) {
        Entry e;
        e.game = &g;
        for (const auto& l : all) {
            const long t = library::title_of(l->titles, g.game.id);
            if (t < 0) continue;
            e.data = l;
            e.title = t;
            break;
        }
        // Its name: the console's, else the title database's, else its
        // catalogue's, else Nlib API's, else its title id.
        e.name = g.name;
        if (e.name.empty())
            if (const auto known = titledb_flow::title(g.game.id)) e.name = known->name;
        if (e.name.empty() && e.data) e.name = e.data->titles[(size_t)e.title].name;
        if (e.name.empty()) e.name = nlib_flow::facts(g.game.id).name;
        if (e.name.empty()) e.name = title_id::text(g.game.id);
        if (something_to_get(state_in(e.data, e.title))) to_get++;
        this->entries.push_back(std::move(e));
    }
    const size_t count = this->entries.size();
    // Those the filter keeps: by their facts, and by what a catalogue has
    // that the console has not.
    auto kept = [](const Entry& e) {
        if (!library::matches(s_filter, nlib_flow::facts(e.game->game.id))) return false;
        const library::TitleState st = state_in(e.data, e.title);
        return !(s_filter.state == library::Filter::State::ToUpdate && !st.update_available) &&
               !(s_filter.state == library::Filter::State::DlcsMissing && !st.dlcs_missing) &&
               !(s_filter.state == library::Filter::State::NeverPlayed && (e.game->launches || e.game->play_s));
    };
    std::erase_if(this->entries, [&](const Entry& e) { return !kept(e); });
    // In the order chosen, by name after it.
    std::stable_sort(this->entries.begin(), this->entries.end(), [](const Entry& a, const Entry& b) {
        if (s_order == Order::ToGetFirst) {
            // Something to get first, the most played of them first.
            const bool x = something_to_get(state_in(a.data, a.title)), y = something_to_get(state_in(b.data, b.title));
            if (x != y) return x;
            if (x && a.game->play_s != b.game->play_s) return a.game->play_s > b.game->play_s;
        } else if (s_order == Order::MostPlayed) {
            if (a.game->play_s != b.game->play_s) return a.game->play_s > b.game->play_s;
        } else if (s_order == Order::RecentlyPlayed) {
            if (a.game->last_played != b.game->last_played) return a.game->last_played > b.game->last_played;
        } else if (s_order == Order::Publisher) {
            const std::string x = library::fold(nlib_flow::facts(a.game->game.id).publisher);
            const std::string y = library::fold(nlib_flow::facts(b.game->game.id).publisher);
            if (x != y) return y.empty() || (!x.empty() && x < y);
        } else if (s_order == Order::Newest) {
            const uint32_t x = nlib_flow::facts(a.game->game.id).release_date, y = nlib_flow::facts(b.game->game.id).release_date;
            if (x != y) return x > y;
        }
        return library::fold(a.name) < library::fold(b.name);
    });
    // The games' facts a filter or the order needs, asked meanwhile.
    if (needs_facts()) {
        std::vector<uint64_t> waiting;
        for (const installed_flow::Game& g : *this->shown_games)
            if (nlib_flow::waiting(g.game.id)) waiting.push_back(g.game.id);
        nlib_flow::enrich(waiting);
    }
    const std::string filtered = filter_sheet::summary(s_filter);
    filter_cell->set_value(aurore::getStr(std::string("playshelf/installed/order_values/") + ORDERS[(int)s_order]) +
                           (filtered.empty() ? "" : " · " + filtered));
    filter_cell->set_active(!filtered.empty());
    ui::set_visible(empty_box, count == 0);
    ui::set_visible(filter_cell, count > 0);
    // Kept even with no game: the focus has a place when the covers go.
    std::string line = count == 1 ? "playshelf/installed/count_one"_i18n : aurore::getStr("playshelf/installed/count", count);
    if (to_get) line += " · " + aurore::getStr("playshelf/installed/to_get", to_get);
    if (this->entries.size() != count) line += " · " + aurore::getStr("playshelf/filters/shown", this->entries.size());
    summary->set_value(line);

    // The focus moves off the covers first: they are made again.
    for (aurore::View* v = aurore::Application::getCurrentFocus(); v; v = v->getParent())
        if (v == grid || v == list_box || v == more) {
            aurore::Application::giveFocus(summary);
            break;
        }
    library_icons::drop_pending();
    grid->clearViews(true);
    if (!this->rows) this->rows = cadran::List::rows_in(list_box);
    this->rows->clearViews(true);
    const bool list = config::get().library_view == "list";
    ui::set_visible(grid, !list);
    ui::set_visible(list_box, list && !this->entries.empty());
    this->icons.clear();
    this->grid_row = nullptr;
    this->built = 0;
    this->generation++;
    this->add_page();
}

void LibraryTab::add_page()
{
    const size_t end = std::min(this->entries.size(), this->built + PAGE);
    const bool   list = config::get().library_view == "list";
    const int    per_row = columns();
    std::vector<uint64_t> shown_ids;
    for (; this->built < end; this->built++) {
        const size_t at = this->built;
        const Entry& e = this->entries[at];
        shown_ids.push_back(e.game->game.id);
        // What stands out: something to get (amber), else its version.
        const library::TitleState st = state_in(e.data, e.title);
        std::string               detail = version_text(*e.game);
        // Ordered by play: how much, unless something is to get.
        if ((s_order == Order::MostPlayed || s_order == Order::RecentlyPlayed) && e.game->play_s)
            detail = installed_flow::play_text(e.game->play_s);
        if (st.update_available) detail = "playshelf/library/update_available"_i18n;
        else if (st.dlcs_missing == 1) detail = "playshelf/installed/dlcs_missing_one"_i18n;
        else if (st.dlcs_missing) detail = aurore::getStr("playshelf/installed/dlcs_missing", st.dlcs_missing);
        const char* color = something_to_get(st) ? "cadran/amber" : "cadran/text3";
        aurore::View*    view = nullptr;
        if (list) {
            auto* cell = new GameCell();
            cell->setText(e.name);
            cell->setDetailText(detail);
            cell->detail->setThemeColor("textColor", color);
            this->rows->addView(cell);
            cadran::List::row(cell);
            this->icons.push_back([cell](const library_icons::Picture& p) {
                if (p) cell->set_icon(p->rgba.data(), p->width, p->height);
            });
            view = cell;
        } else {
            if (!this->grid_row || (int)this->grid_row->getChildren().size() >= per_row) {
                this->grid_row = new aurore::Box(aurore::Axis::ROW);
                grid->addView(this->grid_row);
            }
            auto* tile = new GameTile();
            tile->set_name(e.name);
            tile->set_detail(detail);
            tile->set_detail_color(color);
            // Something to get: marked on the cover too.
            if (something_to_get(st)) tile->set_badge(st.update_available ? "download" : "plus", "cadran/amber");
            this->grid_row->addView(tile);
            this->icons.push_back([tile](const library_icons::Picture& p) {
                if (p) tile->set_icon(p->rgba.data(), p->width, p->height);
            });
            view = tile;
        }
        view->registerClickAction([this, at](aurore::View*) {
            this->open(at);
            return true;
        });
        // Its icon: the console's, else its catalogue's, else the title
        // database's.
        const int  gen = this->generation;
        const auto shown = [weak = ui::Weak(this->alive), this, at, gen](const library_icons::Picture& p) {
            const ui::Alive held = weak.lock();
            if (held && *held && gen == this->generation && at < this->icons.size()) this->icons[at](p);
        };
        if (e.game->icon) library_icons::decode(e.game->icon, shown);
        else if (e.data && !e.data->titles[(size_t)e.title].icon_url.empty())
            library_icons::get(e.data, library::password_of(e.data->server), e.data->titles[(size_t)e.title].icon_url, shown,
                               e.game->game.id);
        else
            library_icons::local(e.game->game.id, shown);
    }
    ui::set_visible(more, this->built < this->entries.size());
    // What Nlib API knows of the games just shown, gathered meanwhile (all
    // of them instead when a filter or the order needs it: show_games()).
    if (!needs_facts()) nlib_flow::enrich(shown_ids);
}

void LibraryTab::pick_filter()
{
    std::vector<uint64_t> games;
    if (this->shown_games)
        for (const installed_flow::Game& g : *this->shown_games) games.push_back(g.game.id);
    filter_sheet::Order order;
    for (const char* o : ORDERS) order.labels.push_back(aurore::getStr(std::string("playshelf/installed/order_values/") + o));
    order.chosen = (int)s_order;
    auto again = ui::guarded(this->alive, [this]() {
        if (this->shown_games) this->show_games(this->shown_games);
    });
    order.pick = [again](int i) {
        s_order = (Order)i;
        again();
    };
    filter_sheet::open(&s_filter, std::move(games), filter_sheet::States::Library, std::move(order), again);
}

void LibraryTab::set_hints()
{

    const bool list = config::get().library_view == "list";
    auto       toggle = [this](aurore::View*) {
        config::get().library_view = config::get().library_view == "list" ? "grid" : "list";
        ui::save_config();
        // On the next frame: the actions being run are this view's own,
        // registered again by set_hints().
        aurore::sync(ui::guarded(this->alive, [this]() {
            if (this->shown_games) this->show_games(this->shown_games);
            this->set_hints();
        }));
        return true;
    };
    this->registerAction(list ? "playshelf/installed/as_grid"_i18n : "playshelf/installed/as_list"_i18n, aurore::BUTTON_Y, toggle);
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    // The desktop build: the keyboard has no Ⓨ, so the Y key.
    this->registerAction(aurore::AuroreKeyCombination(aurore::AURORE_KBD_KEY_Y), toggle);
#endif
    aurore::Application::getGlobalHintsUpdateEvent()->fire();
}

void LibraryTab::open(size_t i)
{
    if (i >= this->entries.size()) return;
    const Entry&      e = this->entries[i];
    const uint64_t    game_id = e.game->game.id;
    const std::string name = e.name;
    // Deleting it, asked first: its base game, update and add-ons; its
    // saves stay.
    std::vector<ui::Choice> lines;
    lines.emplace_back("playshelf/installed/delete"_i18n,
                      ui::guarded(this->alive, [this, game_id, name]() {
                          if (ui::refuse_read_only()) return;
                          ui::confirm_danger(aurore::getStr("playshelf/installed/delete_body", name), "playshelf/installed/delete"_i18n,
                                             ui::guarded(this->alive, [this, game_id, name]() {
                                                 installed_flow::remove(game_id, name, ui::guarded(this->alive,
                                                     [this](const std::shared_ptr<const std::vector<installed_flow::Game>>& games) {
                                                         this->show_games(games);
                                                     }));
                                             }));
                      }),
                      "trash", "", "playshelf/installed/group_console"_i18n, ui::Choice::Tone::DANGER);
    GameActivity::Page page;
    page.name = name;
    page.title_id = game_id;
    if (e.data) {
        // Its server's cheats and save backups, when it is an AeroFoil one.
        const auto services = services_flow::game_choices(e.data, game_id);
        lines.insert(lines.end(), services.begin(), services.end());
        page.lines = library_flow::game_lines(e.data, (size_t)e.title, lines);
    } else {
        // In no catalogue read: what the console has of it.
        const library::InstalledGame& g = e.game->game;
        page.lines.facts = fmt::format("{:016X} · {}", g.id, version_text(*e.game));
        if (g.dlcs.size() == 1) page.lines.facts += " · " + "playshelf/library/dlc_count_one"_i18n;
        else if (!g.dlcs.empty()) page.lines.facts += " · " + aurore::getStr("playshelf/library/dlc_count", g.dlcs.size());
        page.lines.installed = true;
        page.lines.installed_version = std::max(e.game->game.version, e.game->game.update);
        page.note = "playshelf/installed/not_found"_i18n;
        page.lines.more = std::move(lines);
    }
    // More of the console's games of its publisher and of its genre (as
    // far as their facts are known yet), each opening its own page.
    struct Owned
    {
        uint64_t                                    id;
        std::string                                 name;
        std::shared_ptr<const std::vector<uint8_t>> icon;
    };
    std::vector<Owned> console;
    for (size_t k = 0; k < this->entries.size(); k++)
        if (k != i) console.push_back({ this->entries[k].game->game.id, this->entries[k].name, this->entries[k].game->icon });
    auto others = [weak = ui::Weak(this->alive), this, console](const std::function<bool(const library::Facts&)>& like) {
        std::vector<GameActivity::Other> out;
        for (const Owned& o : console) {
            if (!like(nlib_flow::facts(o.id))) continue;
            const uint64_t other = o.id;
            const auto     bytes = o.icon;
            out.push_back({ o.name,
                            [other, bytes](library_icons::Done shown) {
                                if (!bytes) return library_icons::local(other, std::move(shown));
                                library_icons::decode(bytes, [other, shown](const library_icons::Picture& p) {
                                    if (p) shown(p);
                                    else library_icons::local(other, shown);
                                });
                            },
                            [weak, this, other]() {
                                const ui::Alive held = weak.lock();
                                if (!held || !*held) return;
                                for (size_t k = 0; k < this->entries.size(); k++)
                                    if (this->entries[k].game->game.id == other) return this->open(k);
                            } });
        }
        return out;
    };
    page.by_publisher = [others](const std::string& publisher) {
        return others([&](const library::Facts& f) { return f.publisher == publisher; });
    };
    page.by_genre = [others](const std::string& genre) {
        return others([&](const library::Facts& f) {
            return std::ranges::contains(f.categories, genre);
        });
    };
    // Its cover as the Library shows it: the console's icon, else the
    // server's, else the title database's or Nlib API's.
    const std::shared_ptr<const std::vector<uint8_t>> own = e.game->icon;
    const auto                                        data = e.data;
    const std::string url = data ? data->titles[(size_t)e.title].icon_url : "";
    const auto ask = [data, url, game_id](library_icons::Done shown) {
        if (data && !url.empty()) library_icons::get(data, library::password_of(data->server), url, std::move(shown), game_id);
        else library_icons::local(game_id, std::move(shown));
    };
    page.icon = [own, ask](library_icons::Done shown) {
        if (!own) return ask(std::move(shown));
        library_icons::decode(own, [ask, shown](const library_icons::Picture& p) {
            if (p) shown(p);
            else ask(shown);
        });
    };
    GameActivity::show(std::move(page));
}
