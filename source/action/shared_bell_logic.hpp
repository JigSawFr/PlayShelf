// shared_bell_logic — what PlayShelf shares with the other Boréale homebrews
// (boreale-link): its entry in their registry (sd:/config/boreale/apps/,
// bl_app), and the bell they share (sd:/config/boreale/notices/,
// bl_notices). PlayShelf's side, playshelf.json: each entry with the tab
// where it is dealt with as its route (route_of), so that a click in
// another homebrew's bell opens PlayShelf there (--open=<tab>);
// action/notices writes it. Plain C++ so the host tests can run it
// (tests/shared_bell_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace shared_bell_logic
{

// PlayShelf's module: its files' names, as its topics' level would be.
constexpr const char* MODULE = "playshelf";

// The registry's file under `sd_root` ("/" on the console), and its text
// for this version and .nro (from the SD card's root, "/switch/…"; "" keeps
// the one `previous` says), read over `previous`: "" when nothing changed
// (the SD card is not written at each start) or when a newer version wrote
// it.
std::string app_file(const std::string& sd_root);
std::string app_text(const std::string& version, const std::string& nro, const std::string& previous);

// The tab where the entry `id` is dealt with, as a route (one of the main
// screen's tabs: tabs::position_of("tab/" + route)).
std::string route_of(const std::string& id);

struct Entry
{
    std::string id;
    bool        todo = false;   // to do (else for information)
    std::string title;
    std::string detail;
    std::string route;          // its tab (route_of); "" the home
};

// The file under the SD card's root `sd_root` ("/" on the console).
std::string file(const std::string& sd_root);

// The file's text for `entries` in `lang`, PlayShelf's .nro being `nro`
// (from the SD card's root; "" keeps the one `previous` says), read over
// `previous` (the file as it is, "" none): an entry that did not change
// keeps its `updated`, the members PlayShelf does not know are kept. ""
// when nothing changed (the SD card is not written), or when `previous` was
// written by a newer version (never written over).
std::string text(const std::vector<Entry>& entries, const std::string& lang, const std::string& nro,
                 const std::string& previous, int64_t now);

// Another homebrew's notice, as PlayShelf's bell shows it.
struct Notice
{
    std::string id;
    bool        attention = false;   // to do or failed: amber
    std::string title;
    std::string detail;              // with its progress, when it has one ("40 %")
    std::string route;               // the screen it opens at
};

struct Other
{
    std::string         module;   // "playshelf"
    std::string         name;     // its registry's (config/boreale/apps/), else the module
    std::string         nro;      // from the SD card's root, with its '/': "/switch/PlayShelf/PlayShelf.nro"
    std::vector<Notice> notices;
};

// The other homebrews' notices shown at `now` on a console in `lang`, read
// from under `sd_root`: by name; a homebrew whose .nro is no longer there,
// or with nothing to show, left out, and PlayShelf's own file too.
std::vector<Other> others(const std::string& sd_root, const std::string& lang, int64_t now);

}   // namespace shared_bell_logic
