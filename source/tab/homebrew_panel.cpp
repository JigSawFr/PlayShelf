// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/homebrew_panel.hpp"

#include <algorithm>
#include <cctype>

#include "action/library_flow.hpp"
#include "action/library_icons.hpp"
#include "ui/ui.hpp"
#include "view/game_tile.hpp"

using namespace aurore::literals;

namespace
{
bool holds(const std::string& name, const std::string& query)
{
    if (query.empty()) return true;
    auto it = std::search(name.begin(), name.end(), query.begin(), query.end(),
                          [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
    return it != name.end();
}
}   // namespace

HomebrewPanel::HomebrewPanel(aurore::Box* tiles_box, aurore::Label* status_label)
    : host(tiles_box), status(status_label)
{
    this->status->setSingleLine(false);   // a long reason wraps instead of running under the rail
}

HomebrewPanel::~HomebrewPanel()
{
    *this->alive = false;
}

void HomebrewPanel::show(const library::Server& s, const std::string& q, bool only_installed)
{
    if (s.id != this->server.id) {
        this->data = nullptr;
        this->why_failed.clear();
        this->built_key.clear();
    }
    this->server = s;
    this->query = q;
    this->installed_only = only_installed;
    if (!this->data) {
        const auto held = homebrew_flow::loaded();
        if (held && held->server.id == s.id) this->data = held;
    }
    if (!this->data && !this->waiting && this->why_failed.empty()) {
        this->load();
        return;
    }
    this->rebuild();
}

void HomebrewPanel::reload()
{
    this->data = nullptr;
    this->why_failed.clear();
    this->built_key.clear();
    if (!this->waiting) this->load();
}

void HomebrewPanel::load()
{
    this->waiting = true;
    ui::set_text(this->status, "playshelf/homebrew/reading"_i18n);
    const std::string asked = this->server.id;
    homebrew_flow::load(this->server, ui::guarded(this->alive, [this, asked](std::shared_ptr<const homebrew_flow::Loaded> d, std::string why) {
        this->waiting = false;
        // Another server shown meanwhile, or the answer was another one's
        // (a read for the start-up): read again for this one.
        if (asked != this->server.id || (d && d->server.id != this->server.id)) {
            this->data = nullptr;
            this->load();
            return;
        }
        this->present(d, why);
    }));
}

void HomebrewPanel::present(const std::shared_ptr<const homebrew_flow::Loaded>& d, const std::string& why)
{
    this->data = d;
    this->why_failed = why;
    this->built_key.clear();
    this->rebuild();
}

void HomebrewPanel::rebuild()
{
    if (!this->data) {
        this->shown_count = 0;
        if (!this->waiting && !this->why_failed.empty())
            ui::set_text(this->status, aurore::getStr("playshelf/homebrew/failed", this->why_failed));
        return;
    }
    const auto& items = this->data->list.items;
    std::vector<size_t> keep;
    std::string         key = this->query + (this->installed_only ? "\n1\n" : "\n0\n");
    for (size_t i = 0; i < items.size(); i++) {
        const auto standing = homebrew_flow::standing(items[i]);
        if (this->installed_only && standing == homebrew_flow::Standing::New) continue;
        if (!holds(items[i].name, this->query)) continue;
        keep.push_back(i);
        key += std::string(items[i].entry.id) + "|" + std::to_string((int)standing) + "\n";
    }
    this->shown_count = keep.size();
    ui::set_text(this->status, items.empty()    ? "playshelf/homebrew/none"_i18n
                               : keep.empty()   ? " "
                                                : aurore::getStr("playshelf/homebrew/count", keep.size(), this->data->server.name));
    if (key == this->built_key) return;
    this->built_key = key;

    // New tiles in a new box, the focus moved, then the old one removed.
    auto* box = new aurore::Box(aurore::Axis::COLUMN);
    box->setAlignItems(aurore::AlignItems::FLEX_START);
    box->setPaddingLeft((GameTile::room() - (float)GameTile::columns() * GameTile::WIDTH) / 2);
    this->tiles.clear();
    this->entries = keep;
    ++this->generation;
    aurore::Box*  row = nullptr;
    aurore::View* first = nullptr;
    for (size_t at = 0; at < keep.size(); at++) {
        if (!row || (int)row->getChildren().size() >= GameTile::columns()) {
            row = new aurore::Box(aurore::Axis::ROW);
            box->addView(row);
        }
        GameTile* tile = this->make_tile(keep[at]);
        row->addView(tile);
        if (!first) first = tile;
    }
    aurore::View* old = this->host->getChildren().empty() ? nullptr : this->host->getChildren()[0];
    this->host->addView(box);
    if (old) {
        bool inside = false;
        for (aurore::View* v = aurore::Application::getCurrentFocus(); v; v = v->hasParent() ? v->getParent() : nullptr)
            inside |= v == old;
        if (inside && first) aurore::Application::giveFocus(first);
        this->host->removeView(old);
    }
}

GameTile* HomebrewPanel::make_tile(size_t index)
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

// The sheet of an entry: what it is, then what can be done with it.
void HomebrewPanel::open_item(size_t index)
{
    if (!this->data || index >= this->data->list.items.size()) return;
    const auto& item = this->data->list.items[index];
    const auto  st = homebrew_flow::standing(item);
    const std::string item_id(item.entry.id);

    std::string about = item.version;
    if (!item.author.empty()) about += (about.empty() ? "" : " · ") + item.author;
    if (item.size) about += (about.empty() ? "" : " · ") + library_flow::size_text(item.size);
    const std::string& text = item.summary.empty() ? item.description : item.summary;
    if (!text.empty()) about += (about.empty() ? "" : "\n") + text;

    using Choice = ui::Choice;
    std::vector<Choice> lines;
    lines.emplace_back(st == homebrew_flow::Standing::New      ? "playshelf/homebrew/install"_i18n
                       : st == homebrew_flow::Standing::Update ? "playshelf/homebrew/update"_i18n
                                                               : "playshelf/homebrew/reinstall"_i18n,
                       ui::guarded(this->alive, [this, index]() { this->install(index); }), "download");
    // A port needs the user's own game data (SPEC.md 8.6): one more line
    // looks for it on the SD card.
    if (item.entry.port)
        lines.emplace_back("playshelf/homebrew/check_data"_i18n, ui::guarded(this->alive, [this, index]() { this->check_data(index); }),
                           "eye");
    if (st != homebrew_flow::Standing::New)
        lines.emplace_back("playshelf/homebrew/remove"_i18n, ui::guarded(this->alive, [this, item_id]() { this->remove(item_id); }),
                           "trash", "", "", Choice::Tone::DANGER);
    ui::pick_actions(item.name, lines, about);
}

void HomebrewPanel::install(size_t index)
{
    if (ui::refuse_read_only() || !this->data || index >= this->data->list.items.size()) return;
    if (homebrew_flow::busy()) {
        ui::info("playshelf/homebrew/busy"_i18n);
        return;
    }
    const std::string name = this->data->list.items[index].name;
    aurore::Dialog*   dialog = ui::dialog(aurore::getStr("playshelf/homebrew/installing", name));
    dialog->open();
    homebrew_flow::install(this->data, index, ui::guarded(this->alive, [this, dialog, name](std::string why) {
        dialog->close();
        if (why == homebrew_flow::DECLINED) {}
        else if (!why.empty()) ui::error(aurore::getStr("playshelf/homebrew/install_failed", name), why);
        else ui::info(aurore::getStr("playshelf/homebrew/installed_ok", name));
        this->built_key.clear();
        this->rebuild();
    }));
}

void HomebrewPanel::remove(const std::string& id)
{
    if (ui::refuse_read_only()) return;
    const std::string why = homebrew_flow::remove(id);
    if (!why.empty()) ui::error("playshelf/homebrew/remove_failed"_i18n, why);
    this->built_key.clear();
    this->rebuild();
}

void HomebrewPanel::check_data(size_t index)
{
    if (!this->data || index >= this->data->list.items.size()) return;
    const std::string name = this->data->list.items[index].name;
    aurore::Dialog*   dialog = ui::dialog("playshelf/homebrew/checking_data"_i18n);
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
