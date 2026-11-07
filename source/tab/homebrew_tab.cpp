// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/homebrew_tab.hpp"

#include "action/library_flow.hpp"
#include "action/library_icons.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"
#include "view/game_tile.hpp"

using namespace aurore::literals;

namespace
{
// The server read: the first one paired.
std::optional<library::Server> first_server()
{
    for (const library::Server& s : library::load_servers())
        if (!s.uid.empty()) return s;
    return std::nullopt;
}
}   // namespace

HomebrewTab::HomebrewTab()
    : TabBase("xml/tab/homebrew.xml", false)
{
    this->status->setSingleLine(false);   // a long reason wraps instead of running under the rail
}

aurore::View* HomebrewTab::create()
{
    return new HomebrewTab();
}

void HomebrewTab::load()
{
    const auto server = first_server();
    if (!server || this->waiting) return;
    this->waiting = true;
    this->status->setText("playshelf/homebrew/reading"_i18n);
    homebrew_flow::load(*server, ui::guarded(this->alive, [this](std::shared_ptr<const homebrew_flow::Loaded> d, std::string why) {
        this->waiting = false;
        this->present(d, why);
    }));
}

void HomebrewTab::present(const std::shared_ptr<const homebrew_flow::Loaded>& d, const std::string& why)
{
    this->data = d;
    this->why_failed = why;
    this->seen.clear();
    this->refresh();
}

void HomebrewTab::refresh()
{
    if (!first_server()) {
        this->status->setText("playshelf/homebrew/no_server"_i18n);
        return;
    }
    if (!this->data) this->data = homebrew_flow::loaded();
    if (!this->data && !this->waiting && this->why_failed.empty()) {
        this->load();
        return;
    }
    if (!this->data) {
        if (!this->waiting) this->status->setText(aurore::getStr("playshelf/homebrew/failed", this->why_failed));
        return;
    }
    const auto& items = this->data->list.items;
    std::string now;
    for (size_t i = 0; i < items.size(); i++)
        now += std::string(items[i].entry.id) + "|" + std::to_string((int)homebrew_flow::standing(items[i])) + "\n";
    this->status->setText(items.empty() ? "playshelf/homebrew/none"_i18n
                                        : aurore::getStr("playshelf/homebrew/count", items.size(), this->data->server.name));
    if (now == this->seen) return;
    this->seen = now;

    // New tiles in a new box, the focus moved, then the old one removed.
    auto* box = new aurore::Box(aurore::Axis::COLUMN);
    box->setAlignItems(aurore::AlignItems::FLEX_START);
    box->setPaddingLeft((GameTile::room() - (float)GameTile::columns() * GameTile::WIDTH) / 2);
    this->tiles.clear();
    ++this->generation;
    aurore::Box*  row = nullptr;
    aurore::View* first = nullptr;
    for (size_t i = 0; i < items.size(); i++) {
        if (!row || (int)row->getChildren().size() >= GameTile::columns()) {
            row = new aurore::Box(aurore::Axis::ROW);
            box->addView(row);
        }
        GameTile* tile = this->make_tile(i);
        row->addView(tile);
        if (!first) first = tile;
    }
    aurore::View* old = this->list->getChildren().empty() ? nullptr : this->list->getChildren()[0];
    this->list->addView(box);
    if (old) {
        bool inside = false;
        for (aurore::View* v = aurore::Application::getCurrentFocus(); v; v = v->hasParent() ? v->getParent() : nullptr)
            inside |= v == old;
        if (inside && first) aurore::Application::giveFocus(first);
        this->list->removeView(old);
    }
}

GameTile* HomebrewTab::make_tile(size_t index)
{
    const auto& item = this->data->list.items[index];
    auto*       tile = new GameTile();
    tile->set_name(item.name);
    switch (homebrew_flow::standing(item)) {
    case homebrew_flow::Standing::Update:
        tile->set_detail("playshelf/homebrew/update_available"_i18n);
        tile->set_detail_color("cadran/amber");
        tile->set_badge("download", "cadran/amber");
        break;
    case homebrew_flow::Standing::Installed:
        tile->set_detail("playshelf/homebrew/installed"_i18n);
        tile->set_detail_color("cadran/teal");
        tile->set_badge("check", "cadran/teal");
        break;
    default:
        tile->set_detail(item.version);
        tile->set_detail_color("cadran/text3");
        break;
    }
    tile->registerClickAction([this, index](aurore::View*) {
        this->open_item(index);
        return true;
    });
    this->tiles.push_back(tile);
    if (!item.icon.empty()) {
        const size_t at = this->tiles.size() - 1;
        const int    gen = this->generation;
        library_icons::picture(item.icon, 256, 256,
                               [weak = ui::Weak(this->alive), this, at, gen](const library_icons::Picture& p) {
                                   const ui::Alive held = weak.lock();
                                   if (p && held && *held && gen == this->generation && at < this->tiles.size())
                                       this->tiles[at]->set_icon(p->rgba.data(), p->width, p->height);
                               });
    }
    return tile;
}

void HomebrewTab::open_item(size_t index)
{
    if (!this->data || index >= this->data->list.items.size()) return;
    const auto& item = this->data->list.items[index];
    const auto  st = homebrew_flow::standing(item);
    const std::string item_id(item.entry.id);
    std::vector<std::string> values;
    values.push_back(st == homebrew_flow::Standing::New ? "playshelf/homebrew/install"_i18n
                     : st == homebrew_flow::Standing::Update ? "playshelf/homebrew/update"_i18n
                                                            : "playshelf/homebrew/reinstall"_i18n);
    if (st != homebrew_flow::Standing::New) values.push_back("playshelf/homebrew/remove"_i18n);
    // A port needs the user's own game data (SPEC.md 8.6): one more choice
    // looks for it on the SD card.
    const int check_choice = item.entry.port ? (int)values.size() : -1;
    if (check_choice >= 0) values.push_back("playshelf/homebrew/check_data"_i18n);
    std::string title = item.name;
    if (!item.summary.empty()) title += "\n" + item.summary;
    ui::pick(title, values, 0, ui::guarded(this->alive, [this, index, item_id, check_choice](int choice) {
        if (choice == check_choice) {
            this->check_data(index);
            return;
        }
        if (ui::refuse_read_only()) return;
        if (choice == 0) {
            if (homebrew_flow::busy()) {
                ui::info("playshelf/homebrew/busy"_i18n);
                return;
            }
            const std::string name = this->data->list.items[index].name;
            aurore::Dialog* dialog = ui::dialog(aurore::getStr("playshelf/homebrew/installing", name));
            dialog->open();
            homebrew_flow::install(this->data, index, ui::guarded(this->alive, [this, dialog, name](std::string why) {
                dialog->close();
                if (why == homebrew_flow::DECLINED) {}
                else if (!why.empty()) ui::error(aurore::getStr("playshelf/homebrew/install_failed", name), why);
                else ui::info(aurore::getStr("playshelf/homebrew/installed_ok", name));
                this->seen.clear();
                this->refresh();
            }));
        } else {
            const std::string why = homebrew_flow::remove(item_id);
            if (!why.empty()) ui::error("playshelf/homebrew/remove_failed"_i18n, why);
            this->seen.clear();
            this->refresh();
        }
    }));
}

void HomebrewTab::check_data(size_t index)
{
    if (!this->data || index >= this->data->list.items.size()) return;
    const std::string name = this->data->list.items[index].name;
    aurore::Dialog* dialog = ui::dialog("playshelf/homebrew/checking_data"_i18n);
    dialog->open();
    homebrew_flow::check_data(this->data->list.items[index], ui::guarded(this->alive, [dialog, name](lantern_app::PortData got) {
        dialog->close();
        if (!got.error.empty()) {
            ui::error(aurore::getStr("playshelf/homebrew/data_failed", name), got.error);
        } else if (got.ready) {
            ui::info(aurore::getStr(got.unverified ? "playshelf/homebrew/data_unverified" : "playshelf/homebrew/data_ready", name));
        } else {
            std::string lines;
            for (const std::string& m : got.missing) lines += (lines.empty() ? "" : "\n") + m;
            ui::info(aurore::getStr("playshelf/homebrew/data_missing", name, lines));
        }
    }));
}
