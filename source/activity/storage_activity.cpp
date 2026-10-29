// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/storage_activity.hpp"

#include <algorithm>
#include <ctime>
#include <string>
#include <vector>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/cadran_list.hpp>

#include "action/game_page.hpp"
#include "action/installed_flow.hpp"
#include "action/library_flow.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;
namespace kit = cadran::kit;

namespace
{
// When a game was last played, in words.
std::string played_text(const installed_flow::Game& g)
{
    if (!g.last_played && !g.launches) return "playshelf/storage/never"_i18n;
    if (!g.last_played) return "";
    const uint64_t now = (uint64_t)std::time(nullptr);
    const uint64_t days = now > g.last_played ? (now - g.last_played) / 86400 : 0;
    if (days < 1) return "playshelf/storage/today"_i18n;
    if (days < 60) return aurore::getStr("playshelf/storage/days", days);
    return aurore::getStr("playshelf/storage/months", days / 30);
}

// The room taken and left, as a bar: the games' part, the rest, the free part.
aurore::Box* room_bar(const installed_flow::Space& space, uint64_t games)
{
    auto* bar = new aurore::Box(aurore::Axis::ROW);
    bar->setHeight(14);
    bar->setCornerRadius(7);
    bar->setThemeColor("backgroundColor", "cadran/surf2");
    bar->setMarginTop(14);
    const double total = (double)std::max<uint64_t>(space.total, 1);
    const double used = (double)(space.total - std::min(space.free, space.total));
    auto part = [bar](double share, const char* key) {
        if (share <= 0) return;
        auto* p = new aurore::Box();
        p->setHeight(14);
        p->setWidthPercentage((float)std::min(100.0, share * 100.0));
        p->setCornerRadius(7);
        p->setThemeColor("backgroundColor", key);
        bar->addView(p);
    };
    const double games_share = std::min((double)games, used) / total;
    part(games_share, "cadran/teal");
    part(used / total - games_share, "cadran/text3");
    return bar;
}
}   // namespace

StorageActivity::~StorageActivity()
{
    *this->alive = false;
    insights::unlisten(this->listener);
}

void StorageActivity::open()
{
    aurore::Application::pushActivity(new StorageActivity());
}

void StorageActivity::onContentAvailable()
{
    root->setPadding(10, 72, 32, 72);
    this->rebuild();
    this->listener = insights::listen(ui::guarded(this->alive, [this]() {
        // Not under a dialog or a sheet: their cell would go.
        if (ui::has_focus(this)) this->rebuild();
    }));
}

aurore::DetailCell* StorageActivity::game_cell(const insights::Played& p)
{
    auto* cell = new aurore::DetailCell();
    cell->setText(p.name);
    const std::string when = played_text(p.game);
    cell->setDetailText(library_flow::size_text(p.game.size) + (when.empty() ? "" : "  ·  " + when));
    cell->detail->setThemeColor("textColor", "cadran/text2");
    const uint64_t    id = p.game.game.id;
    const std::string name = p.name;
    const auto        found = p.found;
    cell->registerClickAction([id, name, found](aurore::View*) {
        std::vector<ui::Choice> lines;
        if (found.data) {
            const auto   data = found.data;
            const size_t index = found.index;
            lines.emplace_back("playshelf/storage/open_page"_i18n, [data, index]() { game_page::open(data, index); }, "info");
        }
        lines.emplace_back(
            "playshelf/installed/delete"_i18n,
            [id, name]() {
                if (ui::refuse_read_only()) return;
                ui::confirm_danger(aurore::getStr("playshelf/installed/delete_body", name), "playshelf/installed/delete"_i18n,
                                   [id, name]() {
                                       installed_flow::remove(id, name, [](const std::shared_ptr<const std::vector<installed_flow::Game>>&) {
                                           insights::refresh();
                                       });
                                   });
            },
            "trash", "", "", ui::Choice::Tone::DANGER);
        ui::pick_actions(name, lines);
        return true;
    });
    return cell;
}

void StorageActivity::rebuild()
{
    const auto snap = insights::now();
    const auto games = installed_flow::games();

    auto* box = new aurore::Box(aurore::Axis::COLUMN);
    box->setAlignItems(aurore::AlignItems::STRETCH);

    // The card's room: what is free, of how much; the bar.
    std::vector<insights::Played> all;
    uint64_t                      taken = 0;
    if (games)
        for (const installed_flow::Game& g : *games) {
            taken += g.size;
            all.push_back({ g, insights::name_of(g.game.id, g.name), insights::found_of(g.game.id) });
        }
    std::stable_sort(all.begin(), all.end(), [](const insights::Played& a, const insights::Played& b) { return a.game.size > b.game.size; });
    const installed_flow::Space& space = snap->space;
    if (space.known) {
        const bool short_of_room = insights::nearly_full(space);
        box->addView(kit::display(aurore::getStr("playshelf/storage/free_of", library_flow::size_text(space.free),
                                               library_flow::size_text(space.total)),
                                  30, short_of_room ? "cadran/amber" : "cadran/text"));
        box->addView(room_bar(space, taken));
    }
    aurore::Label* line = kit::label(all.size() == 1 ? aurore::getStr("playshelf/storage/games_line_one", library_flow::size_text(taken))
                                                   : aurore::getStr("playshelf/storage/games_line", all.size(), library_flow::size_text(taken)),
                                   17, "cadran/text2");
    line->setMarginTop(10);
    box->addView(line);

    aurore::View*              first = nullptr;
    std::vector<std::string> ids;
    auto header = [box](const std::string& title, const std::string& under) {
        auto* h = new aurore::Header();
        h->setTitle(title);
        h->setMarginTop(28);
        h->setMarginBottom(under.empty() ? 8 : 2);
        box->addView(h);
        if (!under.empty()) {
            aurore::Label* l = kit::label(under, 16, "cadran/text3");
            l->setMarginBottom(10);
            box->addView(l);
        }
    };
    auto add = [&](const insights::Played& p, const char* prefix) {
        aurore::DetailCell* cell = this->game_cell(p);
        ids.push_back(std::string(prefix) + std::to_string(ids.size()));
        cell->setId(ids.back());
        box->addView(cell);
        if (!first) first = cell;
    };
    if (!snap->idle.empty()) {
        header("playshelf/storage/idle"_i18n, "playshelf/storage/idle_line"_i18n);
        for (const insights::Played& p : snap->idle) add(p, "st_idle_");
    }
    if (!all.empty()) {
        header("playshelf/storage/all"_i18n, "");
        for (const insights::Played& p : all) add(p, "st_game_");
    }
    std::vector<std::pair<const char*, const char*>> icons;
    for (const std::string& id : ids) icons.push_back({ id.c_str(), id.rfind("st_idle_", 0) == 0 ? "history" : "gamepad" });
    cadran::List(box, box).apply(icons);

    root->addView(box);
    aurore::Application::giveFocus(first ? first : (aurore::View*)this->getContentView());
    if (this->content) root->removeView(this->content);
    this->content = box;
}
