// The main screen's tabs: their sidebar positions, in the order of
// resources/xml/activity/main.xml (separators count as positions), and the
// screen names their XML gives them ("tab/library").
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace tabs
{

constexpr int library     = 0;   // the games on the console
constexpr int catalogue   = 1;   // a library server's catalogue
constexpr int downloads   = 2;   // the downloads' queue
constexpr int preferences = 4;   // the app's settings and its updates
constexpr int about       = 5;   // version, help, credits

// The n-th entry of config::START_TABS as a sidebar position.
constexpr int of(int n)
{
    constexpr int positions[] = { library, catalogue, preferences, about };
    return n >= 0 && n < (int)(sizeof(positions) / sizeof(positions[0])) ? positions[n] : library;
}

// The position of the tab named `screen` ("tab/about"), -1 for none.
inline int position_of(const std::string& screen)
{
    if (screen == "tab/library") return library;
    if (screen == "tab/catalogue") return catalogue;
    if (screen == "tab/downloads") return downloads;
    if (screen == "tab/preferences") return preferences;
    if (screen == "tab/about") return about;
    return -1;
}

}   // namespace tabs
