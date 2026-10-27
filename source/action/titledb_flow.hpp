// titledb_flow — the title databases (titledb/store) at work: the sources
// of config title_sources, in their order, each in its folder under
// data_dir()/titledb/. At start-up their folders are tidied (a removed
// source's taken away) and the packs opened, off the UI thread; Preferences
// › Title database fetches them again (each manifest over HTTPS, each pack
// over the connections config connections sets, as a download is, and
// checked by its SHA-256 before it is used), one source after the other.
// Names and icons for the games a console or a server does not name: the
// Library's, the Catalogue's.
//
// status(), update(), stop() and the listeners on the UI thread; title()
// and icon() from any thread.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "titledb/pack.hpp"
#include "titledb/store.hpp"

namespace titledb_flow
{

void start();

// What the databases know of a title, nothing before they are open.
std::optional<titledb::Title> title(uint64_t id);
// The base games released between `from` and `to` (YYYYMMDD), `max` at
// most: Home's "Coming soon".
std::vector<uint64_t> released(uint32_t from, uint32_t to, size_t max);
// A title's icon, as an image (WebP, PNG or JPEG) and its kind.
bool icon(uint64_t id, std::vector<uint8_t>* bytes, std::string* ext);

struct Status
{
    bool        open = false;      // the databases read at start-up
    int         opened = 0;        // how many times they were (opened again: new names)
    bool        running = false;   // an update
    size_t      source = 0;        // the source being fetched, of the config's
    uint64_t    done = 0, total = 0;
    std::string error;             // the last update's first failure ("" none)
    bool        changed = false;   // the last update brought something new
    std::vector<titledb::State> sources;   // those open, in the config's order
};
const Status& status();

// Every source fetched again (nothing when one runs already).
void update();
void stop();
// title_sources changed: the folders of the ones removed go, the rest
// opened again in the new order.
void sources_changed();

// Told of every change, on the UI thread; an id for unlisten().
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace titledb_flow
