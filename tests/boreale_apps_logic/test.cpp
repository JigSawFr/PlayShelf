// boreale_apps_logic: the Boréale homebrews read from their registry.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <filesystem>
#include <string>

#include "action/boreale_apps_logic.hpp"
#include "check.h"
#include "temp_dir.hpp"

static void put(const std::string& path, const std::string& text)
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fputs(text.c_str(), f);
    std::fclose(f);
}

int main()
{
    TempDir sd("boreale_apps");
    const std::string root = sd.path();
    CHECK(boreale_apps_logic::installed(root).empty());   // no folder yet

    // PlayShelf, Lantern and Kite installed; Atelier deleted (its .nro gone);
    // Hawk started from nxlink (no .nro known); a file of a newer schema
    // still read; files that are not registry files skipped.
    put(root + "/switch/PlayShelf/PlayShelf.nro", "NRO0");
    put(root + "/config/boreale/apps/playshelf.conf",
        "schema=1\nname=PlayShelf\nversion=1.9.0\nnro=switch/PlayShelf/PlayShelf.nro\n");
    put(root + "/switch/Lantern/Lantern.nro", "NRO0");
    put(root + "/config/boreale/apps/lantern.conf",
        "schema=1\nname=Lantern\nversion=0.3.0\nnro=switch/Lantern/Lantern.nro\nfuture=1\n");
    put(root + "/switch/kite.nro", "NRO0");
    put(root + "/config/boreale/apps/kite.conf", "schema=7\nversion=2.0\nnro=/switch/kite.nro\n");
    put(root + "/config/boreale/apps/atelier.conf", "schema=1\nname=Atelier\nnro=switch/Atelier/Atelier.nro\n");
    put(root + "/config/boreale/apps/hawk.conf", "schema=1\nname=Hawk\n");
    put(root + "/config/boreale/apps/notes.txt", "name=Notes\n");
    put(root + "/config/boreale/apps/Bad Name.conf", "name=Bad\n");

    const auto all = boreale_apps_logic::installed(root);
    if (!CHECK(all.size() == 4)) return CHECK_DONE("boreale_apps_logic");
    CHECK(all[0].self && all[0].module == "playshelf" && all[0].name == "PlayShelf" && all[0].version == "1.9.0");
    CHECK(all[0].nro == "/switch/PlayShelf/PlayShelf.nro");
    CHECK(!all[1].self && all[1].name == "Hawk" && all[1].nro.empty() && all[1].version.empty());
    CHECK(all[2].name == "Lantern" && all[2].version == "0.3.0" && all[2].nro == "/switch/Lantern/Lantern.nro");
    CHECK(all[3].name == "kite" && all[3].module == "kite" && all[3].version == "2.0" && all[3].nro == "/switch/kite.nro");
    CHECK(all[0].repo.empty());
    put(root + "/config/boreale/apps/lantern.conf", "schema=1\nname=Lantern\nversion=0.3.0\nrepo=example/lantern\n");
    CHECK(boreale_apps_logic::installed(root)[2].repo == "example/lantern");

    // The update check made for all: due without a file, then a day later.
    CHECK(boreale_apps_logic::updates_file("/") == "/config/boreale/updates.conf");
    CHECK(boreale_apps_logic::updates_due("", 1000));
    const std::string text = boreale_apps_logic::updates_text("", { { "playshelf", "1.9.1" }, { "lantern", "0.4.0" } }, 1000);
    CHECK(text.find("checked=1000\nlatest.playshelf=1.9.1\nlatest.lantern=0.4.0\n") != std::string::npos);
    CHECK(!boreale_apps_logic::updates_due(text, 1000 + 3600) && boreale_apps_logic::updates_due(text, 1000 + 86400));
    // Lantern not found this time: its last version kept; a newer version's file never written.
    const std::string again = boreale_apps_logic::updates_text(text, { { "playshelf", "1.9.2" } }, 90000);
    CHECK(again.find("checked=90000\nlatest.playshelf=1.9.2\nlatest.lantern=0.4.0\n") != std::string::npos);
    CHECK(boreale_apps_logic::updates_text("schema=2\n", { { "playshelf", "2.0.0" } }, 1).empty());
    return CHECK_DONE("boreale_apps_logic: registry read, sorted, deleted ones left out, update check assertions passed");
}
