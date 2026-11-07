// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/homebrew_tab.hpp"

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
}   // namespace

HomebrewTab::HomebrewTab()
    : TabBase("xml/tab/homebrew.xml", false)
    , panel(std::make_unique<HomebrewPanel>(this->list, this->status))
{
}

aurore::View* HomebrewTab::create()
{
    return new HomebrewTab();
}

void HomebrewTab::refresh()
{
    const auto server = first_server();
    if (!server) {
        ui::set_text(this->status, "playshelf/homebrew/no_server"_i18n);
        return;
    }
    this->panel->show(*server, "", false);
}
