// boreale_apps_logic — the Boréale homebrews installed on the console, from
// their registry (sd:/config/boreale/apps/<module>.conf, boreale-link's
// bl_app, each written by its homebrew when it starts), for About › Boréale
// (tab/boreale_tab). Plain C++ so the host tests can run it
// (tests/boreale_apps_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace boreale_apps_logic
{

struct App
{
    std::string module;    // "playshelf"
    std::string name;      // its registry's, else the module
    std::string version;   // "1.4.0"; "" unknown
    std::string nro;       // from the SD card's root, with its '/': "/switch/PlayShelf/PlayShelf.nro"; "" unknown
    std::string repo;      // where its releases are, "owner/name" on GitHub; "" unknown
    bool        self = false;   // PlayShelf
};

// The homebrews registered under `sd_root`: PlayShelf first, then the others
// by name; one whose .nro is known and no longer there (deleted) left out.
std::vector<App> installed(const std::string& sd_root);

// The update check made for all (config/boreale/updates.conf, boreale-link's
// bl_updates): its file under `sd_root`, and whether a check is due at `now`
// by what the file `text` says ("" none).
std::string updates_file(const std::string& sd_root);
bool        updates_due(const std::string& text, int64_t now);

// The file's text after a check at `now` found `latest` (module, newest
// release; a module left out keeps what the file said): "" when it cannot be
// written (a newer version's file).
std::string updates_text(const std::string& text, const std::vector<std::pair<std::string, std::string>>& latest,
                         int64_t now);

}   // namespace boreale_apps_logic
