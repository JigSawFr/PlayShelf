// repair_plan — which packages of a library server's catalogue to install again to repair a game:
// the game's titles that check_flow found damaged (its base game, its update, an add-on), each
// matched to the catalogue's item of that title at the version the console holds. Plain C++ so the
// host tests can run it (tests/library_repair).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <vector>

#include "library/catalog.hpp"
#include "library/titles.hpp"

namespace library
{

// A title of the game that has a damaged file.
struct Damaged
{
    uint64_t id = 0;        // its title id
    uint32_t version = 0;   // the version the console holds
    Kind     kind = Kind::Unknown;
};

struct RepairPlan
{
    std::vector<size_t> items;     // indexes in the catalogue, each once, in the order of the damaged titles
    std::vector<Damaged> missing;  // damaged titles the catalogue has no package of (at that version)
    uint64_t            bytes = 0; // what the items weigh
};

// A base game is the game's base item; an update, the one of the game's updates at that version; an
// add-on, the game's add-on of that title id (at that version, when the catalogue says one).
RepairPlan plan_repair(const Title& title, const std::vector<Item>& items, const std::vector<Damaged>& damaged);

}   // namespace library
