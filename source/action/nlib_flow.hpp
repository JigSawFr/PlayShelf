// nlib_flow — what Nlib API (titledb/nlib) knows of the games, when config
// nlib_api allows it. A game's details in the language PlayShelf speaks,
// asked off the UI thread and kept a week in cache_dir()/nlib/ (what
// data_cleanup empties); a game it does not know is not asked again until
// PlayShelf starts again. Its banner and screenshots come through
// library_icons' picture().
//
// What they say beyond the text (publisher, developer, date, genres,
// players, languages, rating, demo, eShop id) is kept for good in
// data_dir()/nlib/facts.json (library/facts), gathered as the games show:
// a page opened, and the covers on screen (enrich(): one game at a time,
// those not known yet; a failure of the network stops the rest). The
// Catalogue and the Library sort and filter by it.
//
// details() and the facts: called on the UI thread.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "library/facts.hpp"
#include "titledb/nlib.hpp"

namespace nlib_flow
{

// The facts kept, read off the UI thread (at start-up).
void start();

// The details of the game `title_id` belongs to (its base game's):
// `done(details)` on the UI thread, nullptr when there are none (Nlib API
// off in the settings, out of reach, the game unknown to it).
void details(uint64_t title_id, std::function<void(std::shared_ptr<const titledb::Details>)> done);

// The facts kept so far.
const library::FactsBook& book();
// What is known of a game: its facts kept, else (its publisher, its date)
// the title database's.
library::Facts facts(uint64_t title_id);

// The games on screen: those not known yet asked of Nlib API, one after
// the other, in place of the ones asked before and not done.
void enrich(const std::vector<uint64_t>& title_ids);

// Told on the UI thread when facts came (a few at a time); an id for
// unlisten().
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace nlib_flow
