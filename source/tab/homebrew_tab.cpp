// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/homebrew_tab.hpp"

#include "action/library_flow.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"

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

std::string standing_text(homebrew_flow::Standing s)
{
    switch (s) {
    case homebrew_flow::Standing::Installed: return "playshelf/homebrew/installed"_i18n;
    case homebrew_flow::Standing::Update:    return "playshelf/homebrew/update_available"_i18n;
    default:                                 return {};
    }
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
        this->show(d, why);
    }));
}

void HomebrewTab::show(const std::shared_ptr<const homebrew_flow::Loaded>& d, const std::string& why)
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

    // New cells in a new box, the focus moved, then the old one removed.
    auto* box = new aurore::Box(aurore::Axis::COLUMN);
    box->setAlignItems(aurore::AlignItems::STRETCH);
    aurore::View* first = nullptr;
    for (size_t i = 0; i < items.size(); i++) {
        const auto& item = items[i];
        auto* cell = new aurore::DetailCell();
        cell->setText(item.name);
        std::string detail = item.version;
        if (!item.author.empty()) detail += (detail.empty() ? "" : " · ") + item.author;
        const std::string standing = standing_text(homebrew_flow::standing(item));
        if (!standing.empty()) detail += (detail.empty() ? "" : " · ") + standing;
        cell->setDetailText(detail);
        cell->registerClickAction([this, i](aurore::View*) {
            this->open_item(i);
            return true;
        });
        box->addView(cell);
        if (!first) first = cell;
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

void HomebrewTab::open_item(size_t index)
{
    if (!this->data || index >= this->data->list.items.size()) return;
    const auto& item = this->data->list.items[index];
    const auto  st = homebrew_flow::standing(item);
    const std::string id(item.entry.id);
    std::vector<std::string> values;
    values.push_back(st == homebrew_flow::Standing::New ? "playshelf/homebrew/install"_i18n
                     : st == homebrew_flow::Standing::Update ? "playshelf/homebrew/update"_i18n
                                                            : "playshelf/homebrew/reinstall"_i18n);
    if (st != homebrew_flow::Standing::New) values.push_back("playshelf/homebrew/remove"_i18n);
    std::string title = item.name;
    if (!item.summary.empty()) title += "\n" + item.summary;
    ui::pick(title, values, 0, ui::guarded(this->alive, [this, index, id](int choice) {
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
            const std::string why = homebrew_flow::remove(id);
            if (!why.empty()) ui::error("playshelf/homebrew/remove_failed"_i18n, why);
            this->seen.clear();
            this->refresh();
        }
    }));
}
