// launcher — hands over to a homebrew store to update PlayShelf. It sets the
// homebrew that hbloader starts once this app exits (envSetNextLoad), so it
// works whichever menu (hbmenu, sphaira) PlayShelf was started from.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace launcher
{

enum class Store
{
    None,
    Sphaira,
    AppStore,   // Homebrew App Store (hb-appstore)
};

struct Target
{
    Store       store = Store::None;
    std::string path;   // from the SD card root: "/switch/sphaira/sphaira.nro"
};

// `preference`: "auto" or "" (sphaira, else the App Store), "sphaira",
// "appstore", "manual" or "inplace" (never a store: PlayShelf updates
// itself, action/update_flow). Store::None when nothing usable is installed.
Target find(const std::string& preference);

// Whether the Homebrew App Store installed PlayShelf: libget's record,
// /switch/appstore/.get/packages/PlayShelf/manifest.install, which sphaira's
// App Store keeps too for the official catalogue. Read, never written:
// libget offers an update whenever the record's version differs from the
// catalogue's, so a record set ahead of the catalogue (an update in place it
// has not published yet) would offer to go back.
bool installed_by_store();

// The version of PlayShelf the store's catalogue holds now, from its
// info.json ("1.2.0"): blocking (network), "" with *error set when it cannot
// tell, and with a 404 (*error "not listed") when PlayShelf is not in it. On
// the desktop build PLAYSHELF_SIM_STORE ("1.2.0", "none" or "offline")
// replaces the network (launcher_net.cpp).
std::string store_version(std::string* error);

// The loader can start another homebrew after this one (false on desktop and
// when PlayShelf was not started through hbloader).
bool can_launch();

// Sets `target` as the next homebrew; the caller then quits the app.
bool launch(const Target& target);

// The same for any .nro, from the SD card root ("/switch/playshelf/playshelf.nro"):
// PlayShelf itself after it was replaced (dev_build_flow).
bool launch_nro(const std::string& path);

}   // namespace launcher
