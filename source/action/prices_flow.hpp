// prices_flow — what the games cost on the eShop of the player's country,
// for Home's deals and a game's page: PlayInsights' switch/prices/<COUNTRY>.json
// (library/prices), fetched once a day through jsDelivr while config
// popular_online allows it, kept in cache_dir(). The country is config
// prices_country, else that of PlayShelf's language; none, no prices. Not
// shipped with PlayShelf: a price goes stale, so a cached file older than
// STALE is not shown. The book is the UI thread's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "library/prices.hpp"

namespace prices_flow
{

// The cached file read, then the newest fetched when it is a day old. Once,
// at start-up, and again when the country or popular_online changed.
void start();

// The country whose prices are shown, "" for none.
std::string country();
// Its prices (UI thread); empty while none are known.
std::shared_ptr<const library::PriceBook> book();
// A game's (by its base game's title id), nullptr when unknown.
const library::Price* of(uint64_t base_id);

// Told when the book was read or replaced, on the UI thread.
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace prices_flow
