// library/repair_plan.cpp: which items of a catalogue repair a game.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/repair_plan.hpp"

using namespace library;

namespace
{
Item item(uint64_t id, Kind kind, bool has_version, uint32_t version, uint64_t size)
{
    Item i;
    i.title_id = id;
    i.kind = kind;
    i.has_version = has_version;
    i.version = version;
    i.size = size;
    return i;
}
}   // namespace

int main()
{
    const uint64_t game = 0x0100000000010000ull, update = game | 0x800, dlc1 = game + 0x1001, dlc2 = game + 0x1002;
    // The catalogue: the base game, two updates (newest first), two add-ons.
    const std::vector<Item> items = {
        item(game, Kind::Base, true, 0, 1000),
        item(update, Kind::Update, true, 131072, 300),
        item(update, Kind::Update, true, 65536, 200),
        item(dlc1, Kind::Dlc, true, 0, 50),
        item(dlc2, Kind::Dlc, false, 0, 70),
    };
    Title t;
    t.id = game;
    t.base = 0;
    t.updates = {1, 2};
    t.dlcs = {3, 4};

    {   // each kind finds its item: the update at the version the console holds, not the newest
        const RepairPlan p = plan_repair(t, items, {{game, 0, Kind::Base}, {update, 65536, Kind::Update}, {dlc1, 0, Kind::Dlc}, {dlc2, 7, Kind::Dlc}});
        CHECK(p.items == (std::vector<size_t>{0, 2, 3, 4}));
        CHECK(p.missing.empty() && p.bytes == 1000 + 200 + 50 + 70);
    }
    {   // what the catalogue does not have is said: an update at another version, an add-on it has not, no base game
        const RepairPlan p = plan_repair(t, items, {{update, 196608, Kind::Update}, {game + 0x1003, 0, Kind::Dlc}, {game, 0, Kind::Base}});
        CHECK(p.items == std::vector<size_t>{0});
        CHECK(p.missing.size() == 2 && p.missing[0].id == update && p.missing[1].id == game + 0x1003);
        Title none = t;
        none.base = -1;
        const RepairPlan q = plan_repair(none, items, {{game, 0, Kind::Base}});
        CHECK(q.items.empty() && q.missing.size() == 1 && q.bytes == 0);
    }
    {   // an add-on of another version than the catalogue says is not that item; the same twice counts once
        const RepairPlan p = plan_repair(t, items, {{dlc1, 5, Kind::Dlc}, {game, 0, Kind::Base}, {game, 0, Kind::Base}});
        CHECK(p.missing.size() == 1 && p.missing[0].id == dlc1 && p.items == std::vector<size_t>{0} && p.bytes == 1000);
    }
    {   // an unknown kind, an item index outside the catalogue
        Title odd = t;
        odd.updates = {99};
        const RepairPlan p = plan_repair(odd, items, {{update, 65536, Kind::Update}, {game, 0, Kind::Unknown}});
        CHECK(p.items.empty() && p.missing.size() == 2);
    }
    CHECK_DONE("library repair plan: each damaged title matched to its package, what is missing said passed");
}
