// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/library_tab.hpp"

#include <algorithm>

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/ui/guarded.hpp>

#include "action/download_flow.hpp"
#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "action/titledb_flow.hpp"
#include "activity/game_activity.hpp"
#include "library/servers.hpp"
#include "library/titles.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "view/game_cell.hpp"
#include "view/game_tile.hpp"

using namespace brls::literals;

namespace
{
constexpr size_t PAGE = 40;   // covers built at a time

int columns()
{
    // The tab's room: the screen less the rail and the column's sides.
    const float room = brls::Application::contentWidth - brls::Application::getStyle()["brls/tab_frame/sidebar_width"] -
                       2 * brls::Application::getStyle()["brls/tab_frame/content_padding_sides"];
    return std::max(1, (int)(room / GameTile::WIDTH));
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
    if (!g.version.empty()) return brls::getStr("playshelf/installed/version", g.version);
    return brls::getStr("playshelf/installed/version", library::running_version(g.game));
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
}   // namespace

LibraryTab::LibraryTab()
    : TabBase("xml/tab/library.xml")
{
    cadran::kit::Empty empty = cadran::kit::empty("shelf");
    empty.title->setText("playshelf/installed/empty_title"_i18n);
    empty.text->setText("playshelf/installed/empty_text"_i18n);
    empty_box->addView(empty.box);
    ui::set_visible(empty_box, false);
    summary->registerClickAction([this](brls::View*) {
        this->refresh();
        return true;
    });
    cadran::List::row(summary, "gamepad", true);
    more->registerClickAction([this](brls::View*) {
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
}

LibraryTab::~LibraryTab()
{
    download_flow::unlisten(this->listener);
    titledb_flow::unlisten(this->titledb_listener);
}

brls::View* LibraryTab::create()
{
    return new LibraryTab();
}

void LibraryTab::refresh()
{
    download_flow::show_pill();
    // What the last read had, at once; this read's when it ends.
    if (const auto known = installed_flow::games(); known && known != this->shown_games) this->show_games(known);
    else if (!this->shown_games) ui::set_detail(summary, "playshelf/installed/reading"_i18n);
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
        // catalogue's, else its title id.
        e.name = g.name;
        if (e.name.empty())
            if (const auto known = titledb_flow::title(g.game.id)) e.name = known->name;
        if (e.name.empty()) e.name = e.data ? e.data->titles[(size_t)e.title].name : fmt::format("{:016X}", g.game.id);
        if (something_to_get(state_in(e.data, e.title))) to_get++;
        this->entries.push_back(std::move(e));
    }
    std::stable_sort(this->entries.begin(), this->entries.end(),
                     [](const Entry& a, const Entry& b) { return library::fold(a.name) < library::fold(b.name); });

    const size_t count = this->entries.size();
    ui::set_visible(empty_box, count == 0);
    // Kept even with no game: the focus has a place when the covers go.
    std::string line = count == 1 ? "playshelf/installed/count_one"_i18n : brls::getStr("playshelf/installed/count", count);
    if (to_get) line += " · " + brls::getStr("playshelf/installed/to_get", to_get);
    ui::set_detail(summary, line);

    // The focus moves off the covers first: they are made again.
    for (brls::View* v = brls::Application::getCurrentFocus(); v; v = v->getParent())
        if (v == grid || v == list_box || v == more) {
            brls::Application::giveFocus(summary);
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
        if (st.update_available) detail = "playshelf/library/update_available"_i18n;
        else if (st.dlcs_missing == 1) detail = "playshelf/installed/dlcs_missing_one"_i18n;
        else if (st.dlcs_missing) detail = brls::getStr("playshelf/installed/dlcs_missing", st.dlcs_missing);
        const NVGcolor color = cadran::kit::color(something_to_get(st) ? "cadran/amber" : "cadran/text3");
        brls::View*    view = nullptr;
        if (list) {
            auto* cell = new GameCell();
            cell->setText(e.name);
            cell->setDetailText(detail);
            cell->setDetailTextColor(color);
            this->rows->addView(cell);
            cadran::List::row(cell);
            this->icons.push_back([cell](int texture) { cell->set_icon_texture(texture, true); });
            view = cell;
        } else {
            if (!this->grid_row || (int)this->grid_row->getChildren().size() >= per_row) {
                this->grid_row = new brls::Box(brls::Axis::ROW);
                grid->addView(this->grid_row);
            }
            auto* tile = new GameTile();
            tile->set_name(e.name);
            tile->set_detail(detail);
            tile->set_detail_color(color);
            this->grid_row->addView(tile);
            this->icons.push_back([tile](int texture) { tile->set_icon_texture(texture, true); });
            view = tile;
        }
        view->registerClickAction([this, at](brls::View*) {
            this->open(at);
            return true;
        });
        // Its icon: the console's, else its catalogue's, else the title
        // database's.
        const int  gen = this->generation;
        const auto shown = [weak = ui::Weak(this->alive), this, at, gen](int texture) {
            const ui::Alive held = weak.lock();
            if (held && *held && gen == this->generation && at < this->icons.size()) this->icons[at](texture);
            else if (texture) nvgDeleteImage(brls::Application::getNVGContext(), texture);
        };
        if (e.game->icon) {
            const int texture = nvgCreateImageMem(brls::Application::getNVGContext(), 0, (unsigned char*)e.game->icon->data(),
                                                  (int)e.game->icon->size());
            if (texture > 0) this->icons[at](texture);
        } else if (e.data && !e.data->titles[(size_t)e.title].icon_url.empty())
            library_icons::get(e.data, library::password_of(e.data->server), e.data->titles[(size_t)e.title].icon_url, shown,
                               e.game->game.id);
        else
            library_icons::local(e.game->game.id, shown);
    }
    ui::set_visible(more, this->built < this->entries.size());
    // What Nlib API knows of the games just shown, gathered meanwhile.
    nlib_flow::enrich(shown_ids);
}

void LibraryTab::set_hints()
{

    const bool list = config::get().library_view == "list";
    auto       toggle = [this](brls::View*) {
        config::get().library_view = config::get().library_view == "list" ? "grid" : "list";
        ui::save_config();
        // On the next frame: the actions being run are this view's own,
        // registered again by set_hints().
        brls::sync(ui::guarded(this->alive, [this]() {
            if (this->shown_games) this->show_games(this->shown_games);
            this->set_hints();
        }));
        return true;
    };
    this->registerAction(list ? "playshelf/installed/as_grid"_i18n : "playshelf/installed/as_list"_i18n, brls::BUTTON_Y, toggle);
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    // The desktop build: the keyboard has no Ⓨ, so the Y key.
    this->registerAction(brls::BrlsKeyCombination(brls::BRLS_KBD_KEY_Y), toggle);
#endif
    brls::Application::getGlobalHintsUpdateEvent()->fire();
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
                          ui::confirm_danger(brls::getStr("playshelf/installed/delete_body", name), "playshelf/installed/delete"_i18n,
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
        library_flow::GameLines got = library_flow::game_lines(e.data, (size_t)e.title, lines);
        page.facts = std::move(got.facts);
        page.lines = std::move(got.lines);
    } else {
        // In no catalogue read: what the console has of it.
        const library::InstalledGame& g = e.game->game;
        page.facts = fmt::format("{:016X} · {}", g.id, version_text(*e.game));
        if (g.dlcs.size() == 1) page.facts += " · " + "playshelf/library/dlc_count_one"_i18n;
        else if (!g.dlcs.empty()) page.facts += " · " + brls::getStr("playshelf/library/dlc_count", g.dlcs.size());
        page.note = "playshelf/installed/not_found"_i18n;
        page.lines = std::move(lines);
    }
    // Its cover as the Library shows it: the console's icon, else the
    // server's, else the title database's or Nlib API's.
    const std::shared_ptr<const std::vector<uint8_t>> own = e.game->icon;
    const auto                                        data = e.data;
    const std::string url = data ? data->titles[(size_t)e.title].icon_url : "";
    page.icon = [own, data, url, game_id](std::function<void(int)> shown) {
        if (own) {
            const int texture = nvgCreateImageMem(brls::Application::getNVGContext(), 0, (unsigned char*)own->data(), (int)own->size());
            if (texture > 0) return shown(texture);
        }
        if (data && !url.empty()) library_icons::get(data, library::password_of(data->server), url, std::move(shown), game_id);
        else library_icons::local(game_id, std::move(shown));
    };
    GameActivity::show(std::move(page));
}
