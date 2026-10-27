// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/catalogue_tab.hpp"

#include <algorithm>
#include <map>

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_frame.hpp>
#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_modal.hpp>

#include "action/download_flow.hpp"
#include "action/game_page.hpp"
#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "library/servers.hpp"
#include "tab/filter_sheet.hpp"
#include "ui/ui.hpp"
#include "view/game_tile.hpp"
#include "view/tool_chip.hpp"

using namespace brls::literals;

namespace
{
constexpr size_t PAGE = 40;   // covers built at a time

// What is chosen stays for this run (a tab is made again each time it opens).
std::string   s_server_id;
std::string   s_query;
size_t        s_order = 0;   // in ORDERS
std::string   s_section;
library::Filter s_filter;

// The orders, as library::Sort's, then the shelves: a row of covers a
// genre, by name.
const char* const ORDERS[] = { "name", "newest", "largest", "publisher", "shelves" };
constexpr size_t  SHELVES = 4;
constexpr size_t  SHELF_MAX = 12;   // covers a shelf, "See all" after them

bool shelves()
{
    return s_order == SHELVES;
}

library::Sort sort_of()
{
    return shelves() ? library::Sort::Name : (library::Sort)s_order;
}

// A filter or an order that needs the games' facts (Nlib API's, gathered
// as games show): the ones not known yet are asked meanwhile.
bool needs_facts()
{
    const library::Filter& f = s_filter;
    return !f.genre.empty() || !f.publisher.empty() || !f.language.empty() || f.players || f.hide_demos ||
           sort_of() == library::Sort::Publisher || shelves();
}

std::string order_text(size_t order)
{
    return brls::getStr(std::string("playshelf/library/sort_values/") + ORDERS[order]);
}

// What a cover says under the name: installed (an update to get, in
// amber), else its size and what the server has besides the base game.
void show_detail(GameTile* tile, const library::Title& t, const library::Catalog& catalog,
                 const library_flow::Installed& installed)
{
    const library::TitleState st = library::state_of(t, catalog, installed);
    if (st.base) {
        tile->set_detail(st.update_available ? "playshelf/library/update_available"_i18n : "playshelf/library/installed"_i18n);
        tile->set_detail_color(st.update_available ? "cadran/amber" : "cadran/teal");
        tile->set_badge(st.update_available ? "download" : "check", st.update_available ? "cadran/amber" : "cadran/teal");
        return;
    }
    tile->set_badge(nullptr, nullptr);
    tile->set_detail_color("cadran/text3");
    std::string text = library_flow::size_text(t.size);
    if (t.base < 0) text += " · " + "playshelf/library/no_base"_i18n;
    else if (t.dlcs.size() == 1) text += " · " + "playshelf/library/dlc_count_one"_i18n;
    else if (!t.dlcs.empty()) text += " · " + brls::getStr("playshelf/library/dlc_count", t.dlcs.size());
    tile->set_detail(text);
}

// The tab's room: the screen less the rail and the column's sides.
float room()
{
    return brls::Application::contentWidth - brls::Application::getStyle()["brls/tab_frame/sidebar_width"] -
           2 * brls::Application::getStyle()["brls/tab_frame/content_padding_sides"];
}

int columns()
{
    return std::max(1, (int)(room() / GameTile::WIDTH));
}

bool inside(brls::View* view, brls::View* parent)
{
    for (brls::View* v = view; v; v = v->getParent())
        if (v == parent) return true;
    return false;
}
}   // namespace

CatalogueTab::CatalogueTab()
    : TabBase("xml/tab/catalogue.xml", false), list(this)
{
    cadran::kit::Empty empty = cadran::kit::empty("shelf");
    empty.title->setText("playshelf/library/empty_title"_i18n);
    empty.text->setText("playshelf/library/empty_text"_i18n);
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setMarginTop(20);
    row->setJustifyContent(brls::JustifyContent::CENTER);
    auto button = [row](const std::string& text, const std::function<void()>& run) {
        auto* b = new brls::Button();
        b->setText(text);
        cadran::style_button(b);
        b->setMarginLeft(8);
        b->setMarginRight(8);
        b->registerClickAction([run](brls::View*) {
            run();
            return true;
        });
        row->addView(b);
        cadran::kit::fit_button(b);
    };
    auto saved = ui::guarded(this->alive, [this](const std::string& saved_id) { this->after_saved(saved_id); });
    button("playshelf/library/add_server"_i18n, [saved]() { library_flow::add(saved); });
    button("playshelf/library/discover"_i18n, [saved]() { library_flow::discover(saved); });
    cadran::kit::wrap(row);
    empty.box->addView(row);
    empty.box->setMarginTop(40);
    empty_box->addView(empty.box);

    // The queue's changes: the header's line, and the covers once an
    // install ended.
    this->downloads_listener = download_flow::listen(ui::guarded(this->alive, [this]() {
        if (this->shown()) this->refresh();
    }));
    // The controls, a pill each.
    server_cell = new ToolChip("server", "");
    search_cell = new ToolChip("eye", "playshelf/library/search"_i18n);
    sort_cell = new ToolChip("list", "playshelf/library/sort"_i18n);
    filter_cell = new ToolChip("tools", "playshelf/filters/row"_i18n);
    section_cell = new ToolChip("folder", "playshelf/library/section"_i18n);
    for (ToolChip* c : { server_cell, search_cell, sort_cell, filter_cell, section_cell }) bar->addView(c);
    cadran::kit::wrap(bar);
    server_cell->registerClickAction([this](brls::View*) {
        this->server_sheet();
        return true;
    });
    search_cell->registerClickAction([this](brls::View*) {
        this->search();
        return true;
    });
    sort_cell->registerClickAction([this](brls::View*) {
        this->pick_sort();
        return true;
    });
    filter_cell->registerClickAction([this](brls::View*) {
        this->pick_filter();
        return true;
    });
    // Facts came while a filter or an order needs them: the games again.
    this->nlib_listener = nlib_flow::listen(ui::guarded(this->alive, [this]() { this->facts_came(); }));
    section_cell->registerClickAction([this](brls::View*) {
        this->pick_section();
        return true;
    });
    more->registerClickAction([this](brls::View*) {
        // The focus stays on the button's place: the first new cover takes it.
        const size_t first = this->built;
        this->add_page();
        if (first < this->built && this->grid_row) {
            brls::View* line = this->grid->getChildren()[first / (size_t)columns()];
            brls::Application::giveFocus(dynamic_cast<brls::Box*>(line)->getChildren()[first % (size_t)columns()]);
        }
        return true;
    });
    this->registerAction("playshelf/library/search"_i18n, brls::BUTTON_Y, [this](brls::View*) {
        if (this->server()) this->search();
        return true;
    });
    this->registerAction("playshelf/library/reload"_i18n, brls::BUTTON_X, [this](brls::View*) {
        if (this->server()) this->load(true);
        return true;
    });
    cadran::style_button(more);
}

const library::Server* CatalogueTab::server() const
{
    if (const library::Server* s = library::find_server(this->servers, s_server_id)) return s;
    return this->servers.empty() ? nullptr : &this->servers.front();
}

CatalogueTab::~CatalogueTab()
{
    download_flow::unlisten(this->downloads_listener);
    nlib_flow::unlisten(this->nlib_listener);
}

void CatalogueTab::refresh()
{
    download_flow::show_pill();
    this->servers = library::load_servers();
    const library::Server* s = this->server();
    if (s) s_server_id = s->id;
    ui::set_visible_all({ { empty_box, !s }, { server_cell, s != nullptr }, { search_cell, s != nullptr },
                          { sort_cell, s != nullptr }, { filter_cell, s != nullptr } });
    if (!s) {
        ui::set_visible_all({ { section_cell, false }, { status, false }, { grid, false }, { more, false } });
        this->shown_data = nullptr;
        return;
    }
    server_cell->set_value(s->name);
    search_cell->set_value(s_query.empty() ? "playshelf/library/search_all"_i18n : s_query);
    search_cell->set_active(!s_query.empty());
    sort_cell->set_value(order_text(s_order));
    const std::string filtered = filter_sheet::summary(s_filter);
    filter_cell->set_value(filtered.empty() ? "playshelf/filters/none"_i18n : filtered);
    filter_cell->set_active(!filtered.empty());
    ui::set_visible_all({ { status, true }, { grid, true } });

    this->shown_data = library_flow::loaded(s->id);
    if (!this->shown_data && this->why_failed.empty()) this->load(false);
    this->build_grid(false);
    this->show_status();
    this->list.sync();
}

void CatalogueTab::load(bool again)
{
    const library::Server* s = this->server();
    // A fetch already running (this tab made again meanwhile): told when
    // it ends, once.
    if (!s || this->waiting || (again && library_flow::loading())) return;
    if (again) library_flow::forget(s->id);
    this->why_failed.clear();
    this->shown_data = nullptr;
    this->waiting = true;
    const std::string asked = s->id;
    library_flow::load(*s, ui::guarded(this->alive, [this, asked](const std::shared_ptr<const library_flow::Loaded>& l, const std::string& why) {
        this->waiting = false;
        // Another server shown meanwhile: its catalogue read now (this one's
        // stays for when it comes back).
        if (asked == s_server_id) this->why_failed = l ? "" : why.empty() ? "playshelf/library/not_trusted"_i18n : why;
        this->refresh();
    }));
    this->show_status();
    this->build_grid(true);
}

void CatalogueTab::show_status()
{
    std::string text;
    const char* color = "cadran/text3";
    if (library_flow::loading()) {
        text = "playshelf/library/loading"_i18n;
    } else if (!this->why_failed.empty()) {
        text = brls::getStr("playshelf/library/failed", this->why_failed);
        color = "cadran/coral";
    } else if (this->shown_data) {
        const library::Fetch& f = this->shown_data->fetch;
        text = brls::getStr(f.on_lan ? "playshelf/library/count_lan" : "playshelf/library/count_wan",
                            this->selection.size(), this->shown_data->titles.size());
        if (needs_facts()) {
            size_t known = 0;
            for (const library::Title& t : this->shown_data->titles)
                if (t.id && nlib_flow::book().find(t.id)) known++;
            text += "\n" + brls::getStr("playshelf/filters/known", known, this->shown_data->titles.size());
        }
        if (!f.catalog.motd.empty()) text += "\n" + f.catalog.motd;
    }
    status->setThemeColor("textColor", color);
    ui::set_text(status, text.empty() ? " " : text);
}

void CatalogueTab::build_grid(bool force)
{
    const auto* data = this->shown_data.get();
    const library::Filter& f = s_filter;
    const std::string      key = fmt::format("{}\n{}\n{}\n{}\n{}\n{}\n{}{}{}", s_query, s_order, s_section, f.genre, f.publisher,
                                             f.language, f.players, (int)f.state, f.hide_demos);
    // Its sections: a choice only when the server has several.
    const bool sections = data && data->fetch.catalog.sections.size() > 1;
    ui::set_visible(section_cell, sections);
    if (sections) {
        std::string name = "playshelf/library/section_all"_i18n;
        for (const library::Section& sec : data->fetch.catalog.sections)
            if (sec.id == s_section) name = sec.title;
        section_cell->set_value(name);
        section_cell->set_active(!s_section.empty());
    }
    const auto installed = library_flow::installed();
    if (!force && data == this->built_data && key == this->built_key) {
        // Only what is installed changed (an install ended): the covers'
        // lines again, in place (one of them has the focus).
        if (installed != this->built_installed && data) {
            this->built_installed = installed;
            for (size_t i = 0; i < this->tiles.size(); i++)
                show_detail(this->tiles[i], data->titles[this->tile_titles[i]], data->fetch.catalog, *installed);
        }
        return;
    }
    this->built_data = data;
    this->built_key = key;
    this->built_installed = installed;

    if (inside(brls::Application::getCurrentFocus(), grid)) brls::Application::giveFocus(search_cell);
    grid->clearViews(true);
    this->tiles.clear();
    this->tile_titles.clear();
    // The icons of the covers gone are not wanted; one already on its way
    // finds its cover gone (the generation changed) and is dropped.
    library_icons::drop_pending();
    this->generation++;
    this->password = data ? library::password_of(data->server) : "";
    this->grid_row = nullptr;
    this->built = 0;
    // The covers centred in the tab's room; a shelf scrolls across it all.
    grid->setPaddingLeft(shelves() ? 0 : (room() - (float)columns() * GameTile::WIDTH) / 2);
    this->selection = this->select();
    this->ask_facts();
    if (shelves()) this->add_shelves();
    else this->add_page();
    this->show_status();
}

std::vector<size_t> CatalogueTab::select() const
{
    const auto* data = this->shown_data.get();
    if (!data) return {};
    const auto         installed = this->built_installed;
    library::Selection pick;
    pick.filter = s_filter;
    pick.facts = [](uint64_t title_id) { return nlib_flow::facts(title_id); };
    pick.state = [data, installed](const library::Title& t) { return library::state_of(t, data->fetch.catalog, *installed); };
    const bool sections = data->fetch.catalog.sections.size() > 1;
    return library::select_titles(data->titles, s_query, sort_of(), sections ? s_section : "", pick);
}

void CatalogueTab::ask_facts()
{
    // The games not known yet, asked a page at a time while a filter or
    // the order needs their facts.
    const auto* data = this->shown_data.get();
    if (!data || !needs_facts()) return;
    std::vector<uint64_t> unknown;
    for (const library::Title& t : data->titles)
        if (t.id && nlib_flow::waiting(t.id) && unknown.size() < PAGE) unknown.push_back(t.id);
    nlib_flow::enrich(unknown);
}

void CatalogueTab::facts_came()
{
    if (!this->shown() || !this->shown_data) return;
    if (!needs_facts()) return;
    // The covers built again only when what they show changed (one may
    // have the focus); the next games asked either way.
    if (this->select() != this->selection) this->build_grid(true);
    else this->ask_facts();
    this->show_status();
}

void CatalogueTab::add_page()
{
    const auto* data = this->shown_data.get();
    const size_t end = data ? std::min(this->selection.size(), this->built + PAGE) : 0;
    const int    per_row = columns();
    std::vector<uint64_t> shown_ids;
    for (; this->built < end; this->built++) {
        if (!this->grid_row || (int)this->grid_row->getChildren().size() >= per_row) {
            this->grid_row = new brls::Box(brls::Axis::ROW);
            grid->addView(this->grid_row);
        }
        const size_t index = this->selection[this->built];
        if (data->titles[index].id) shown_ids.push_back(data->titles[index].id);
        this->grid_row->addView(this->make_tile(index));
    }
    ui::set_visible(more, data && this->built < this->selection.size());
    // What Nlib API knows of the games just shown, gathered meanwhile (the
    // ones a filter needs are asked instead: build_grid()).
    if (!needs_facts()) nlib_flow::enrich(shown_ids);
}

GameTile* CatalogueTab::make_tile(size_t index)
{
    const auto*           data = this->shown_data.get();
    const library::Title& t = data->titles[index];
    auto*                 tile = new GameTile();
    tile->set_name(t.name);
    show_detail(tile, t, data->fetch.catalog, *this->built_installed);
    tile->registerClickAction([this, index](brls::View*) {
        this->open_title(index);
        return true;
    });
    this->tiles.push_back(tile);
    this->tile_titles.push_back(index);
    const size_t at = this->tiles.size() - 1;
    const int    gen = this->generation;
    auto shown = [weak = ui::Weak(this->alive), this, at, gen](const library_icons::Picture& p) {
        const ui::Alive held = weak.lock();
        if (p && held && *held && gen == this->generation && at < this->tiles.size())
            this->tiles[at]->set_icon(p->rgba.data(), p->width, p->height);
    };
    // The server's icon, else the title database's, else Nlib API's.
    if (!t.icon_url.empty()) library_icons::get(this->shown_data, this->password, t.icon_url, shown, t.id);
    else library_icons::local(t.id, shown);
    return tile;
}

void CatalogueTab::add_shelves()
{
    ui::set_visible(more, false);
    const auto* data = this->shown_data.get();
    if (!data) return;
    // The games of each genre (a game on each of its genres' shelves), the
    // fullest shelf first; the games not known yet on the last one.
    std::map<std::string, std::vector<size_t>> by_genre;
    std::vector<size_t>                        unknown;
    for (size_t index : this->selection) {
        const library::Facts facts = nlib_flow::facts(data->titles[index].id);
        if (facts.categories.empty()) unknown.push_back(index);
        for (const std::string& g : facts.categories) by_genre[g].push_back(index);
    }
    std::vector<std::pair<std::string, std::vector<size_t>>> shelves(by_genre.begin(), by_genre.end());
    std::stable_sort(shelves.begin(), shelves.end(), [](const auto& a, const auto& b) {
        // "Other" with the games not known, at the end.
        if ((a.first == "Other") != (b.first == "Other")) return b.first == "Other";
        return a.second.size() > b.second.size();
    });
    if (!unknown.empty()) shelves.emplace_back("", std::move(unknown));
    for (auto& [genre, games] : shelves) {
        // Its genre in the display face, how many games in a soft chip.
        auto* header = new brls::Box(brls::Axis::ROW);
        header->setAlignItems(brls::AlignItems::CENTER);
        header->setMarginTop(this->tiles.empty() ? 12 : 28);
        header->setMarginBottom(4);
        header->setMarginLeft(8);
        header->addView(cadran::kit::display(
            genre.empty() ? "playshelf/library/shelf_unknown"_i18n : nlib_flow::genre_name(genre), 24,
            genre.empty() ? "cadran/text2" : "cadran/text"));
        auto* count = new brls::Box(brls::Axis::ROW);
        count->setHeight(26);
        count->setAlignItems(brls::AlignItems::CENTER);
        count->setPaddingLeft(10);
        count->setPaddingRight(10);
        count->setCornerRadius(13);
        count->setMarginLeft(12);
        count->setThemeColor("backgroundColor", genre.empty() ? "cadran/surf2" : "cadran/teal_soft");
        count->addView(cadran::kit::label(std::to_string(games.size()), 15, genre.empty() ? "cadran/text2" : "cadran/teal"));
        header->addView(count);
        grid->addView(header);
        auto* frame = new brls::HScrollingFrame();
        frame->setWidth(room());
        frame->setHeight(GameTile::WIDTH + 64);
        auto* row = new brls::Box(brls::Axis::ROW);
        frame->addView(row);
        grid->addView(frame);
        for (size_t i = 0; i < games.size() && i < SHELF_MAX; i++) row->addView(this->make_tile(games[i]));
        // The whole genre: the grid, filtered by it.
        if (games.size() > SHELF_MAX && !genre.empty()) {
            auto* all = new GameTile();
            all->set_name("playshelf/library/shelf_all"_i18n);
            all->set_detail(brls::getStr("playshelf/library/shelf_games", games.size()));
            all->set_detail_color("cadran/teal");
            const std::string g = genre;
            // The grid built again once this click is over (it frees this cover).
            all->registerClickAction([this, g](brls::View*) {
                brls::sync(ui::guarded(this->alive, [this, g]() {
                    s_filter.genre = g;
                    s_order = 0;
                    this->refresh();
                }));
                return true;
            });
            row->addView(all);
        }
    }
}

void CatalogueTab::open_title(size_t index)
{
    game_page::open(this->shown_data, index);
}

void CatalogueTab::server_sheet()
{
    using Choice = ui::Choice;
    std::vector<Choice> choices;
    auto saved = ui::guarded(this->alive, [this](const std::string& saved_id) { this->after_saved(saved_id); });
    for (const library::Server& s : this->servers) {
        if (s.id == s_server_id) continue;
        const std::string other = s.id;
        choices.emplace_back(s.name, ui::guarded(this->alive, [this, other]() {
                                 s_server_id = other;
                                 this->why_failed.clear();
                                 s_section.clear();
                                 this->refresh();
                             }),
                             "server", "", "playshelf/library/group_servers"_i18n);
    }
    if (const library::Server* s = this->server()) {
        const library::Server copy = *s;
        choices.emplace_back("playshelf/library/reload"_i18n, ui::guarded(this->alive, [this]() { this->load(true); }),
                             "refresh", "", "playshelf/library/group_this"_i18n);
        choices.emplace_back("playshelf/library/edit_server"_i18n, [copy, saved]() { library_flow::edit(copy, saved); },
                             "pencil", "", "playshelf/library/group_this"_i18n);
    }
    if (this->servers.size() < library::MAX_SERVERS) {
        choices.emplace_back("playshelf/library/add_server"_i18n, [saved]() { library_flow::add(saved); }, "plus", "",
                             "playshelf/library/group_add"_i18n);
        choices.emplace_back("playshelf/library/discover"_i18n, [saved]() { library_flow::discover(saved); }, "globe",
                             "", "playshelf/library/group_add"_i18n);
    }
    const library::Server* s = this->server();
    ui::pick_actions("playshelf/library/server"_i18n, choices, s ? s->name : "");
}

void CatalogueTab::pick_sort()
{
    std::vector<std::string> labels;
    for (size_t i = 0; i < sizeof(ORDERS) / sizeof(ORDERS[0]); i++) labels.push_back(order_text(i));
    ui::pick("playshelf/library/sort"_i18n, labels, (int)s_order, ui::guarded(this->alive, [this](int i) {
                 s_order = (size_t)i;
                 this->refresh();
             }));
}

void CatalogueTab::pick_filter()
{
    const auto data = this->shown_data;
    std::vector<uint64_t> games;
    if (data)
        for (const library::Title& t : data->titles)
            if (t.id) games.push_back(t.id);
    filter_sheet::open(&s_filter, std::move(games), filter_sheet::States::Catalogue, {},
                       ui::guarded(this->alive, [this]() { this->refresh(); }));
}

void CatalogueTab::pick_section()
{
    const auto data = this->shown_data;
    if (!data) return;
    std::vector<std::string> labels = { "playshelf/library/section_all"_i18n };
    std::vector<std::string> ids = { "" };
    int                      selected = 0;
    for (const library::Section& sec : data->fetch.catalog.sections) {
        if (sec.id == s_section) selected = (int)ids.size();
        labels.push_back(sec.title.empty() ? sec.id : sec.title);
        ids.push_back(sec.id);
    }
    ui::pick("playshelf/library/section"_i18n, labels, selected, ui::guarded(this->alive, [this, ids](int i) {
                 s_section = ids[(size_t)i];
                 this->refresh();
             }));
}

void CatalogueTab::search()
{
    ui::prompt_text("playshelf/library/search"_i18n, s_query, 64, ui::guarded(this->alive, [this](const std::string& q) {
                        s_query = q;
                        this->refresh();
                    }));
}

void CatalogueTab::after_saved(const std::string& saved_id)
{
    // Saved: shown (and read again, its catalogue forgotten); deleted: the
    // first one left.
    s_server_id = saved_id;
    this->why_failed.clear();
    s_section.clear();
    this->refresh();
}

brls::View* CatalogueTab::create()
{
    return new CatalogueTab();
}
