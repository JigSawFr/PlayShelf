// The main screen's tabs: their sidebar positions, in the order of
// resources/xml/activity/main.xml (separators count as positions), and the
// screen names their XML gives them ("tab/library").
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <iterator>
#include <string>

namespace tabs
{

constexpr int home        = 0;   // where the console and its servers stand
constexpr int library     = 1;   // the games on the console
constexpr int catalogue   = 2;   // a library server's catalogue
constexpr int downloads   = 3;   // the downloads' queue
constexpr int preferences = 5;   // the app's settings and its updates
constexpr int about       = 6;   // version, help, credits

// The n-th entry of config::START_TABS as a sidebar position.
constexpr int of(int n)
{
    constexpr int positions[] = { home, library, catalogue, preferences, about };
    return n >= 0 && n < (int)std::size(positions) ? positions[n] : library;
}

// The position of the tab named `screen` ("tab/about"), -1 for none.
inline int position_of(const std::string& screen)
{
    if (screen == "tab/home") return home;
    if (screen == "tab/library") return library;
    if (screen == "tab/catalogue") return catalogue;
    if (screen == "tab/downloads") return downloads;
    if (screen == "tab/preferences") return preferences;
    if (screen == "tab/about") return about;
    return -1;
}

}   // namespace tabs
