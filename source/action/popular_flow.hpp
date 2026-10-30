// popular_flow — what Atlas (https://github.com/JigSawFr/Atlas)
// knows of the games, for Home and a game's page: how well known each is
// (popular.json: library/popular), its traits, Steam audience and IGDB
// players' rating (games.json), the games most like it (similar.json) and
// the game an edition is one of (editions.json: library/signals).
// Each as PlayShelf came with it and, once a day when config popular_online
// allows it, the newest from Atlas' data branch (its platforms/switch/ folder) through jsDelivr
// (kept in cache_dir()), whichever was made last. Read off the UI thread at
// start-up; the books are the UI thread's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "library/popular.hpp"
#include "library/signals.hpp"

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

// games.json's, similar.json's and editions.json's, empty before they are
// read (editions.json does not ship with PlayShelf: fetched only).
std::shared_ptr<const library::SignalBook>  signals();
std::shared_ptr<const library::SimilarBook> similar();
std::shared_ptr<const library::EditionBook> editions();

// Told when a book was read or replaced, on the UI thread.
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace popular_flow
