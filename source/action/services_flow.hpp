// services_flow — an AeroFoil server's services beyond its catalogue
// (library/services), from the Catalogue tab and a game's sheet, on the
// network in the background, each with the address, credentials and pin
// its catalogue was read with:
// - its cheats: a game's builds, installed in Atmosphère's folder of the SD
//   card (atmosphere/contents/<title id>/cheats/<build id>.txt), shown,
//   removed; every build at once;
// - a user's save backups: a game's save sent with a note, a backup put
//   back in place of the save (the save it replaces kept on the SD card
//   first, and put back should that fail), a backup deleted;
// - the games asked of it: the user's requests and how each stands, a game
//   asked for, found by its name in the title databases.
// The server's web sign-in that its cheats' list and its requests need is
// kept for the run, by server.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "action/library_flow.hpp"

namespace services_flow
{

// Whether the server `l` was read from has these services (AeroFoil's API).
bool offered(const library_flow::Loaded& l);

// A game's lines for its page (`title_id`'s of `l`): its cheats and its
// save backups; none when the server has no such services.
std::vector<ui::Choice> game_choices(const std::shared_ptr<const library_flow::Loaded>& l, uint64_t title_id);

// The server's cheats: every game's (a list of games first), or `title_id`'s.
void cheats(const std::shared_ptr<const library_flow::Loaded>& l, uint64_t title_id = 0);
// A user's save backups (the user chosen first when the console has
// several): every game's, or `title_id`'s.
void saves(const std::shared_ptr<const library_flow::Loaded>& l, uint64_t title_id = 0);
// The user's requests, and a game asked for.
void requests(const std::shared_ptr<const library_flow::Loaded>& l);

}   // namespace services_flow
