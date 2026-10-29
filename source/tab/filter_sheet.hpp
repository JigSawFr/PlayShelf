// filter_sheet — the sheet the Catalogue and the Library filter their games
// with (library::Filter): a genre and a publisher among those the games'
// facts know (action/nlib_flow: gathered as games show, so a game not known
// yet matches none), how many play, the language PlayShelf speaks, what the
// console has of them, demos hidden; and the order, for the Library. Each
// choice applied at once, the sheet open again on it; Clear when one is set.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "library/facts.hpp"

namespace filter_sheet
{

// What the sheet offers of the console's state: the Catalogue's games
// (not on the console, on it, an update or add-ons to get) or the
// Library's (on the console: an update or add-ons to get).
enum class States { Catalogue, Library };

// An order offered at the sheet's top (its label; `chosen`: the one in use;
// `pick`: another one taken); none when `labels` is empty.
struct Order
{
    std::vector<std::string>  labels;
    int                       chosen = 0;
    std::function<void(int)> pick;
};

// The sheet for `*filter` (left alive by the caller: a tab's own), offering
// the genres and publishers of `games` (title ids). `changed()` after each
// choice.
void open(library::Filter* filter, std::vector<uint64_t> games, States states, Order order,
          std::function<void()> changed);

// What is set, in a few words ("Adventure · Northfox · +1"; "" for nothing).
std::string summary(const library::Filter& filter);

}   // namespace filter_sheet
