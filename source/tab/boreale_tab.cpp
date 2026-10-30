// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/boreale_tab.hpp"

#include <aurore/cadran/ui/cadran_list.hpp>

#include "ui/ui.hpp"
#include "util/launcher.hpp"
#include "util/paths.hpp"

using namespace aurore::literals;

BorealeTab::BorealeTab()
    : TabBase("xml/tab/boreale.xml", true)
{
    intro->setSingleLine(false);
}

std::string BorealeTab::names()
{
    std::string out;
    for (const auto& app : boreale_apps_logic::installed(paths::sd_root()))
        out += (out.empty() ? "" : ", ") + app.name;
    return out;
}

aurore::DetailCell* BorealeTab::cell(const boreale_apps_logic::App& app)
{
    auto* cell = new aurore::DetailCell();
    cell->setText(app.name);
    std::string detail = app.version.empty() ? "" : aurore::getStr("playshelf/cadran/about/version", app.version);
    if (app.self) {
        detail += (detail.empty() ? "" : " · ") + "playshelf/boreale/this_app"_i18n;
        cell->setDetailText(detail);
        cell->setFocusable(false);
        return cell;
    }
    cell->setDetailText(ui::opens(detail));
    const std::string name = app.name, nro = app.nro, module = app.module;
    cell->registerClickAction([name, nro, module](aurore::View*) {
        if (!nro.empty() && launcher::can_launch() && launcher::launch_nro(nro)) {
            aurore::Logger::info("boreale: opens {} ({})", module, nro);
            aurore::Application::quit();   // hbloader starts it as soon as PlayShelf exits
            return true;
        }
        ui::info(aurore::getStr("playshelf/boreale/open_manual", name));
        return true;
    });
    return cell;
}

void BorealeTab::refresh()
{
    const auto apps = boreale_apps_logic::installed(paths::sd_root());
    // Built again only when it changed (each time the screen comes back,
    // after a dialog too: the focus stays where it was).
    std::string now;
    for (const auto& app : apps) now += app.module + "\n" + app.name + "\n" + app.version + "\n" + app.nro + "\n";
    if (now == this->seen) return;
    this->seen = now;

    // New cells in a new box, the focus moved to it, then the old one
    // removed: Aurore would keep a pointer to a deleted focused cell.
    auto* box = new aurore::Box(aurore::Axis::COLUMN);
    box->setAlignItems(aurore::AlignItems::STRETCH);
    aurore::View* first = nullptr;
    std::vector<std::string> ids;
    for (const auto& app : apps) {
        aurore::DetailCell* c = this->cell(app);
        ids.push_back("bo_" + app.module);
        c->setId(ids.back());
        box->addView(c);
        if (!first && c->isFocusable()) first = c;
    }
    // In a card, each with its sign: PlayGuard's and PlayShelf's own, a
    // gamepad for the others.
    std::vector<std::pair<const char*, const char*>> icons;
    for (size_t i = 0; i < apps.size(); i++)
        icons.push_back({ ids[i].c_str(), apps[i].module == "playguard" ? "shield"
                                          : apps[i].module == "playshelf" ? "shelf" : "gamepad" });
    cadran::List(box, box).apply(icons);
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
