// popular_flow — what Atlas (https://github.com/JigSawFr/Atlas)
// knows of the games, for Home and a game's page: how well known each is
// (popular.json: library/popular), its name, its traits, Steam audience and IGDB
// players' rating (games.json), the games most like it (similar.json) and
// the game an edition is one of (editions.json: library/signals).
// popular.json and games.json as PlayShelf came with them; each, once a day
// when config popular_online allows it, the newest from Atlas' data branch
// (its platforms/switch/ folder) through jsDelivr (kept in cache_dir()),
// whichever was made last. similar.json and editions.json are only fetched:
// empty until they first come. Read off the UI thread at start-up; the books
// are the UI thread's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "library/popular.hpp"
#include "library/signals.hpp"

namespace popular_flow
{

// The file PlayShelf came with (popular.json, games.json), the cached one,
// then (a day after the last time, or one never fetched, popular_online on)
// the newest one fetched. Once, at start-up.
void start();
// Fetched again now (Preferences turned it on).
void refresh();

// A game's, by its base game's title id (UI thread); a score of 0 when unknown.
library::Renown of(uint64_t base_id);
// The games known (0 before the file is read).
size_t size();
// A game's name as Atlas' games.json tells it (titledb's), "" when unknown:
// for one no server, title database or Nlib API names (UI thread).
std::string name_of(uint64_t base_id);

// games.json's, similar.json's and editions.json's, empty before they are
// read (similar.json and editions.json do not ship with PlayShelf: empty
// until the first fetch, and for good with popular_online off).
std::shared_ptr<const library::SignalBook>  signals();
std::shared_ptr<const library::SimilarBook> similar();
std::shared_ptr<const library::EditionBook> editions();

// Told when a book was read or replaced, on the UI thread.
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace popular_flow
