// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/library_tab.hpp"

#include <fmt/format.h>

#include "ui/cadran_frame.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/cadran_modal.hpp"
#include "ui/ui.hpp"
#include "view/game_tile.hpp"

using namespace brls::literals;

namespace
{
constexpr size_t PAGE = 40;   // covers built at a time

// What is chosen stays for this run (a tab is made again each time it opens).
std::string   s_server_id;
std::string   s_query;
library::Sort s_sort = library::Sort::Name;
std::string   s_section;

const char* const SORTS[] = { "name", "newest", "largest" };

std::string size_text(uint64_t bytes)
{
    const double mb = (double)bytes / (1024.0 * 1024.0);
    std::string  text = mb >= 1024.0 ? fmt::format("{:.1f}", mb / 1024.0) : fmt::format("{:.0f}", mb);
    const size_t dot = text.find('.');
    if (dot != std::string::npos) text.replace(dot, 1, "playshelf/games/decimal_mark"_i18n);
    return brls::getStr(mb >= 1024.0 ? "playshelf/library/size_gb" : "playshelf/library/size_mb", text);
}

std::string sort_text(library::Sort s)
{
    return brls::getStr(std::string("playshelf/library/sort_values/") + SORTS[(int)s]);
}

// What a cover says under the name: its size, and what the server has
// besides the base game.
std::string tile_detail(const library::Title& t)
{
    std::string text = size_text(t.size);
    if (t.base < 0) text += " · " + "playshelf/library/no_base"_i18n;
    else if (t.dlcs.size() == 1) text += " · " + "playshelf/library/dlc_count_one"_i18n;
    else if (!t.dlcs.empty()) text += " · " + brls::getStr("playshelf/library/dlc_count", t.dlcs.size());
    return text;
}

int columns()
{
    // The tab's room: the screen less the rail and the column's sides.
    const float room = brls::Application::contentWidth - brls::Application::getStyle()["brls/tab_frame/sidebar_width"] -
                       2 * brls::Application::getStyle()["brls/tab_frame/content_padding_sides"];
    return std::max(1, (int)(room / GameTile::WIDTH));
}

bool inside(brls::View* view, brls::View* parent)
{
    for (brls::View* v = view; v; v = v->getParent())
        if (v == parent) return true;
    return false;
}
}   // namespace

LibraryTab::LibraryTab()
    : TabBase("xml/tab/library.xml", false), list(this)
{
    cadran::kit::Empty empty = cadran::kit::empty("shelf");
    empty.title->setText("playshelf/library/empty_title"_i18n);
    empty.text->setText("playshelf/library/empty_text"_i18n);
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setMarginTop(20);
    row->setJustifyContent(brls::JustifyContent::CENTER);
    auto button = [row](const std::string& text, std::function<void()> run) {
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

    this->list.apply({ { "lb_server", "server" }, { "lb_search", "eye" }, { "lb_sort", "list" }, { "lb_section", "folder" } });
}

const library::Server* LibraryTab::server() const
{
    if (const library::Server* s = library::find_server(this->servers, s_server_id)) return s;
    return this->servers.empty() ? nullptr : &this->servers.front();
}

void LibraryTab::refresh()
{
    cadran::set_pill("");
    this->servers = library::load_servers();
    const library::Server* s = this->server();
    if (s) s_server_id = s->id;
    ui::set_visible_all({ { empty_box, !s }, { server_cell, s != nullptr }, { search_cell, s != nullptr },
                          { sort_cell, s != nullptr } });
    if (!s) {
        ui::set_visible_all({ { section_cell, false }, { status, false }, { grid, false }, { more, false } });
        this->shown_data = nullptr;
        return;
    }
    ui::set_detail(server_cell, s->name);
    ui::set_detail(search_cell, s_query.empty() ? "playshelf/library/search_all"_i18n : s_query);
    ui::set_detail(sort_cell, sort_text(s_sort));
    ui::set_visible_all({ { status, true }, { grid, true } });

    this->shown_data = library_flow::loaded(s->id);
    if (!this->shown_data && this->why_failed.empty()) this->load(false);
    this->build_grid(false);
    this->show_status();
    this->list.sync();
}

void LibraryTab::load(bool again)
{
    const library::Server* s = this->server();
    if (!s || library_flow::loading()) return;
    if (again) library_flow::forget(s->id);
    this->why_failed.clear();
    this->shown_data = nullptr;
    const std::string asked = s->id;
    library_flow::load(*s, ui::guarded(this->alive, [this, asked](std::shared_ptr<const library_flow::Loaded> l, std::string why) {
        // Another server shown meanwhile: its catalogue read now (this one's
        // stays for when it comes back).
        if (asked == s_server_id) this->why_failed = l ? "" : why.empty() ? "playshelf/library/not_trusted"_i18n : why;
        this->refresh();
    }));
    this->show_status();
    this->build_grid(true);
}

void LibraryTab::show_status()
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
        if (!f.catalog.motd.empty()) text += "\n" + f.catalog.motd;
    }
    status->setTextColor(cadran::kit::color(color));
    ui::set_text(status, text.empty() ? " " : text);
}

void LibraryTab::build_grid(bool force)
{
    const auto* data = this->shown_data.get();
    const std::string key = s_query + "\n" + std::to_string((int)s_sort) + "\n" + s_section;
    // Its sections: a choice only when the server has several.
    const bool sections = data && data->fetch.catalog.sections.size() > 1;
    ui::set_visible(section_cell, sections);
    if (sections) {
        std::string name = "playshelf/library/section_all"_i18n;
        for (const library::Section& sec : data->fetch.catalog.sections)
            if (sec.id == s_section) name = sec.title;
        ui::set_detail(section_cell, name);
    }
    if (!force && data == this->built_data && key == this->built_key) return;
    this->built_data = data;
    this->built_key = key;

    if (inside(brls::Application::getCurrentFocus(), grid)) brls::Application::giveFocus(search_cell);
    grid->clearViews(true);
    this->grid_row = nullptr;
    this->built = 0;
    this->selection = data ? library::select_titles(data->titles, s_query, s_sort, sections ? s_section : "")
                           : std::vector<size_t>();
    this->add_page();
    this->show_status();
}

void LibraryTab::add_page()
{
    const auto* data = this->shown_data.get();
    const size_t end = data ? std::min(this->selection.size(), this->built + PAGE) : 0;
    const int    per_row = columns();
    for (; this->built < end; this->built++) {
        if (!this->grid_row || (int)this->grid_row->getChildren().size() >= per_row) {
            this->grid_row = new brls::Box(brls::Axis::ROW);
            grid->addView(this->grid_row);
        }
        const size_t          index = this->selection[this->built];
        const library::Title& t = data->titles[index];
        auto*                 tile = new GameTile();
        tile->set_name(t.name);
        tile->set_detail(tile_detail(t));
        tile->registerClickAction([this, index](brls::View*) {
            this->open_title(index);
            return true;
        });
        this->grid_row->addView(tile);
    }
    ui::set_visible(more, data && this->built < this->selection.size());
}

void LibraryTab::open_title(size_t index)
{
    const auto data = this->shown_data;
    if (!data || index >= data->titles.size()) return;
    const library::Title&             t = data->titles[index];
    const std::vector<library::Item>& items = data->fetch.catalog.items;
    using Choice = ui::Choice;
    std::vector<Choice> choices;
    auto install = [data](std::vector<size_t> chosen) {
        return [data, chosen]() { library_flow::install(*data, chosen); };
    };

    // Everything at once: the base game, its newest update, its add-ons.
    std::vector<size_t> all;
    if (t.base >= 0) all.push_back((size_t)t.base);
    if (library::latest_update(t) >= 0) all.push_back((size_t)library::latest_update(t));
    all.insert(all.end(), t.dlcs.begin(), t.dlcs.end());
    uint64_t all_size = 0;
    for (size_t i : all) all_size += items[i].size;
    if (all.size() > 1)
        choices.emplace_back("playshelf/library/install_all"_i18n, install(all), "download", size_text(all_size));

    if (t.base >= 0) {
        const library::Item& it = items[(size_t)t.base];
        choices.emplace_back("playshelf/library/base"_i18n, install({ (size_t)t.base }), "gamepad",
                             size_text(it.size), "playshelf/library/group_base"_i18n);
    }
    for (size_t i : t.updates) {
        const library::Item& it = items[i];
        choices.emplace_back(brls::getStr("playshelf/library/update", it.version), install({ i }), "refresh",
                             size_text(it.size), "playshelf/library/group_updates"_i18n);
    }
    for (size_t i : t.dlcs) {
        const library::Item& it = items[i];
        choices.emplace_back(library::clean_name(it.name), install({ i }), "plus", size_text(it.size),
                             "playshelf/library/group_dlcs"_i18n);
    }
    const std::string subtitle = t.id ? fmt::format("{:016X} · {}", t.id, size_text(t.size)) : size_text(t.size);
    ui::pick_actions(t.name, choices, subtitle);
}

void LibraryTab::server_sheet()
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

void LibraryTab::pick_sort()
{
    std::vector<std::string> labels;
    for (int i = 0; i < 3; i++) labels.push_back(sort_text((library::Sort)i));
    ui::pick("playshelf/library/sort"_i18n, labels, (int)s_sort, ui::guarded(this->alive, [this](int i) {
                 s_sort = (library::Sort)i;
                 this->refresh();
             }));
}

void LibraryTab::pick_section()
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

void LibraryTab::search()
{
    ui::prompt_text("playshelf/library/search"_i18n, s_query, 64, ui::guarded(this->alive, [this](const std::string& q) {
                        s_query = q;
                        this->refresh();
                    }));
}

void LibraryTab::after_saved(const std::string& saved_id)
{
    // Saved: shown (and read again, its catalogue forgotten); deleted: the
    // first one left.
    s_server_id = saved_id;
    this->why_failed.clear();
    s_section.clear();
    this->refresh();
}

brls::View* LibraryTab::create()
{
    return new LibraryTab();
}
