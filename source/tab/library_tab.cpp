// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/library_tab.hpp"

#include <algorithm>

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/ui/guarded.hpp>

#include "action/download_flow.hpp"
#include "action/library_icons.hpp"
#include "library/servers.hpp"
#include "library/titles.hpp"
#include "ui/ui.hpp"
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
}

LibraryTab::~LibraryTab()
{
    download_flow::unlisten(this->listener);
}

brls::View* LibraryTab::create()
{
    return new LibraryTab();
}

void LibraryTab::refresh()
{
    download_flow::show_pill();
    // What the last read had, at once; this read's when it ends.
    if (const auto known = installed_flow::games(); known && known != this->shown_games) this->show(known);
    else if (!this->shown_games) ui::set_detail(summary, "playshelf/installed/reading"_i18n);
    // A catalogue to compare the games with: the first server's when none
    // is read yet.
    const std::vector<library::Server> servers = library::load_servers();
    if (!servers.empty() && catalogues().empty() && !library_flow::loading())
        library_flow::load(servers.front(), ui::guarded(this->alive, [this](const std::shared_ptr<const library_flow::Loaded>& l, const std::string&) {
            if (l && this->shown_games) this->show(this->shown_games);
        }));
    installed_flow::read(ui::guarded(this->alive, [this](const std::shared_ptr<const std::vector<installed_flow::Game>>& games) {
        this->show(games);
    }));
}

void LibraryTab::show(std::shared_ptr<const std::vector<installed_flow::Game>> games)
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
        e.name = !g.name.empty() ? g.name : e.data ? e.data->titles[(size_t)e.title].name : fmt::format("{:016X}", g.game.id);
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
        if (v == grid || v == more) {
            brls::Application::giveFocus(summary);
            break;
        }
    library_icons::drop_pending();
    grid->clearViews(true);
    this->tiles.clear();
    this->grid_row = nullptr;
    this->built = 0;
    this->generation++;
    this->add_page();
}

void LibraryTab::add_page()
{
    const size_t end = std::min(this->entries.size(), this->built + PAGE);
    const int    per_row = columns();
    for (; this->built < end; this->built++) {
        if (!this->grid_row || (int)this->grid_row->getChildren().size() >= per_row) {
            this->grid_row = new brls::Box(brls::Axis::ROW);
            grid->addView(this->grid_row);
        }
        const size_t at = this->built;
        const Entry& e = this->entries[at];
        auto*        tile = new GameTile();
        tile->set_name(e.name);
        // What stands out: something to get (amber), else its version.
        const library::TitleState st = state_in(e.data, e.title);
        if (st.update_available) tile->set_detail("playshelf/library/update_available"_i18n);
        else if (st.dlcs_missing == 1) tile->set_detail("playshelf/installed/dlcs_missing_one"_i18n);
        else if (st.dlcs_missing) tile->set_detail(brls::getStr("playshelf/installed/dlcs_missing", st.dlcs_missing));
        else tile->set_detail(version_text(*e.game));
        tile->set_detail_color(cadran::kit::color(something_to_get(st) ? "cadran/amber" : "cadran/text3"));
        tile->registerClickAction([this, at](brls::View*) {
            this->open(at);
            return true;
        });
        this->grid_row->addView(tile);
        this->tiles.push_back(tile);
        // Its icon: the console's, else its catalogue's.
        if (e.game->icon) {
            const int texture = nvgCreateImageMem(brls::Application::getNVGContext(), 0, (unsigned char*)e.game->icon->data(),
                                                  (int)e.game->icon->size());
            if (texture > 0) tile->set_icon_texture(texture, true);
        } else if (e.data && !e.data->titles[(size_t)e.title].icon_url.empty()) {
            const int gen = this->generation;
            library_icons::get(e.data, library::password_of(e.data->server), e.data->titles[(size_t)e.title].icon_url,
                               [weak = ui::Weak(this->alive), this, at, gen](int texture) {
                                   const ui::Alive held = weak.lock();
                                   if (held && *held && gen == this->generation && at < this->tiles.size())
                                       this->tiles[at]->set_icon_texture(texture, true);
                                   else if (texture) nvgDeleteImage(brls::Application::getNVGContext(), texture);
                               });
        }
    }
    ui::set_visible(more, this->built < this->entries.size());
}

void LibraryTab::open(size_t i)
{
    if (i >= this->entries.size()) return;
    const Entry& e = this->entries[i];
    if (e.data) {
        library_flow::game_sheet(e.data, (size_t)e.title);
        return;
    }
    // In no catalogue read: what the console has of it.
    const library::InstalledGame& g = e.game->game;
    std::string                   text = fmt::format("{:016X} · {}", g.id, version_text(*e.game));
    if (g.dlcs.size() == 1) text += " · " + "playshelf/library/dlc_count_one"_i18n;
    else if (!g.dlcs.empty()) text += " · " + brls::getStr("playshelf/library/dlc_count", g.dlcs.size());
    ui::info(e.name + "\n" + text + "\n\n" + "playshelf/installed/not_found"_i18n);
}
