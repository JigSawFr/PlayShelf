// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/homebrew_panel.hpp"

#include <algorithm>
#include <cctype>

#include "action/library_flow.hpp"
#include "action/library_icons.hpp"
#include "ui/ui.hpp"
#include "view/game_tile.hpp"
#include "view/tool_chip.hpp"

using namespace aurore::literals;

namespace homebrew_kind {

void show(ToolChip* chip, bool homebrew)
{
    chip->set_value(homebrew ? "playshelf/tabs/homebrew"_i18n : "playshelf/library/kind_games"_i18n);
    chip->set_active(homebrew);
}

void pick(bool homebrew, std::function<void(bool)> then)
{
    const std::vector<std::string> labels = { "playshelf/library/kind_games"_i18n, "playshelf/tabs/homebrew"_i18n };
    ui::pick("playshelf/library/kind"_i18n, labels, homebrew ? 1 : 0, [then](int i) { then(i == 1); });
}

} // namespace homebrew_kind

namespace
{
bool holds(const std::string& name, const std::string& query)
{
    if (query.empty()) return true;
    auto it = std::search(name.begin(), name.end(), query.begin(), query.end(),
                          [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
    return it != name.end();
}

bool lower_before(const std::string& a, const std::string& b)
{
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                        [](char x, char y) { return std::tolower((unsigned char)x) < std::tolower((unsigned char)y); });
}

// `a` before `b`: by name always in the end; the ones with an update first
// (then the ones not on the console yet), or the largest first.
bool before(const lantern_app::HomebrewItem& a, const lantern_app::HomebrewItem& b, HomebrewPanel::Order order)
{
    if (order == HomebrewPanel::Order::Updates) {
        const int ra = homebrew_flow::standing(a) == homebrew_flow::Standing::Update ? 0 : 1;
        const int rb = homebrew_flow::standing(b) == homebrew_flow::Standing::Update ? 0 : 1;
        if (ra != rb) return ra < rb;
    } else if (order == HomebrewPanel::Order::Largest && a.size != b.size) {
        return a.size > b.size;
    }
    return lower_before(a.name, b.name);
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

void HomebrewPanel::show(const library::Server& s, const std::string& q, bool only_installed, Order by)
{
    if (s.id != this->server.id) {
        this->data = nullptr;
        this->why_failed.clear();
        this->built_key.clear();
    }
    this->server = s;
    this->query = q;
    this->installed_only = only_installed;
    this->order = by;
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
    for (size_t i = 0; i < items.size(); i++) {
        const auto standing = homebrew_flow::standing(items[i]);
        if (this->installed_only && standing == homebrew_flow::Standing::New) continue;
        if (!holds(items[i].name, this->query)) continue;
        keep.push_back(i);
    }
    std::stable_sort(keep.begin(), keep.end(), [&](size_t a, size_t b) { return before(items[a], items[b], this->order); });
    std::string key = this->query + (this->installed_only ? "\n1\n" : "\n0\n") + std::to_string((int)this->order) + "\n";
    for (size_t i : keep) key += std::string(items[i].entry.id) + "|" + std::to_string((int)homebrew_flow::standing(items[i])) + "\n";
    this->shown_count = keep.size();
    ui::set_text(this->status, items.empty()    ? "playshelf/homebrew/none"_i18n
                               : keep.empty()   ? (this->installed_only && this->query.empty() ? "playshelf/homebrew/none_installed"_i18n : " ")
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
    size_t        updates = 0;
    for (size_t i : keep) updates += homebrew_flow::standing(items[i]) == homebrew_flow::Standing::Update;
    if (updates > 0) {
        // One pill to bring every update, above the tiles.
        auto* all = new ToolChip("download", "playshelf/homebrew/update_all"_i18n);
        all->set_value(std::to_string(updates));
        all->set_active(true);
        all->setMarginBottom(12);
        all->registerClickAction([this](aurore::View*) {
            this->update_all();
            return true;
        });
        box->addView(all);
        first = all;
    }
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
        else {
            std::string line = aurore::getStr("playshelf/homebrew/installed_ok", name);
            if (!homebrew_flow::origin().empty()) line += "\n" + homebrew_flow::origin();
            ui::info(line);
        }
        this->built_key.clear();
        this->rebuild();
    }));
}

void HomebrewPanel::update_all()
{
    if (ui::refuse_read_only() || !this->data) return;
    if (homebrew_flow::busy()) {
        ui::info("playshelf/homebrew/busy"_i18n);
        return;
    }
    auto todo = std::make_shared<std::vector<size_t>>();
    for (size_t i = 0; i < this->data->list.items.size(); i++)
        if (homebrew_flow::standing(this->data->list.items[i]) == homebrew_flow::Standing::Update) todo->push_back(i);
    if (!todo->empty()) this->update_step(todo, 0, 0);
}

// The `at`th of `todo`; on to the next when it is done or its paths were
// refused, stopped with the reason when it fails. `done` counts the ones
// installed.
void HomebrewPanel::update_step(const std::shared_ptr<std::vector<size_t>>& todo, size_t at, size_t done)
{
    if (at >= todo->size()) {
        this->built_key.clear();
        this->rebuild();
        if (done > 0) ui::info(aurore::getStr("playshelf/homebrew/updated_all", done));
        return;
    }
    const std::string name = this->data->list.items[(*todo)[at]].name;
    aurore::Dialog*   dialog = ui::dialog(aurore::getStr("playshelf/homebrew/updating", name, at + 1, todo->size()));
    dialog->open();
    homebrew_flow::install(this->data, (*todo)[at], ui::guarded(this->alive, [this, dialog, name, todo, at, done](std::string why) {
        dialog->close();
        this->built_key.clear();
        if (!why.empty() && why != homebrew_flow::DECLINED) {
            ui::error(aurore::getStr("playshelf/homebrew/install_failed", name), why);
            this->rebuild();
            return;
        }
        this->update_step(todo, at + 1, done + (why.empty() ? 1 : 0));
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
