// popular_flow — how well known each game is (library/popular), for Home:
// popular.json as PlayShelf came with it, and once a day, when config
// popular_online allows it, the newest one from the project's repository
// (jsDelivr's copy of resources/data/popular.json on main, kept in
// cache_dir()), whichever was made last. Read off the UI thread at start-up.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>

#include "library/popular.hpp"

namespace popular_flow
{

// The file PlayShelf came with, the cached one, then (a day after the last
// time, popular_online on) the newest one fetched. Once, at start-up.
void start();
// Fetched again now (Preferences turned it on).
void refresh();

// A game's, by its base game's title id (UI thread); a score of 0 when unknown.
library::Renown of(uint64_t base_id);
// The games known (0 before the file is read).
size_t size();

// Told when the book was read or replaced, on the UI thread.
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace popular_flow
