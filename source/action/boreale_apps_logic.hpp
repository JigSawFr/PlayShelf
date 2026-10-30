// boreale_apps_logic — the Boréale homebrews installed on the console, from
// their registry (sd:/config/boreale/apps/<module>.conf, boreale-link's
// bl_app, each written by its homebrew when it starts), for About › Boréale
// (tab/boreale_tab). Plain C++ so the host tests can run it
// (tests/boreale_apps_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace boreale_apps_logic
{

struct App
{
    std::string module;    // "playshelf"
    std::string name;      // its registry's, else the module
    std::string version;   // "1.4.0"; "" unknown
    std::string nro;       // from the SD card's root, with its '/': "/switch/PlayShelf/PlayShelf.nro"; "" unknown
    bool        self = false;   // PlayShelf
};

// The homebrews registered under `sd_root`: PlayShelf first, then the others
// by name; one whose .nro is known and no longer there (deleted) left out.
std::vector<App> installed(const std::string& sd_root);

}   // namespace boreale_apps_logic
