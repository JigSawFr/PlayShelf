// local_saves_flow — a game's saves copied to the SD card (saves/local_backups): the save of every
// user of the console that has one, as a zip, the last few of each user kept. Made by hand from the
// storage screen, and by itself before an update installs (download_flow).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace local_saves_flow
{

struct Outcome
{
    size_t      copies = 0;   // the zips made
    std::string error;        // the first thing that failed ("" when none)
};

// The copies of game `title_id` (its base game's id), then the old ones deleted. Reads the saves and
// writes the SD card: on a worker thread. A user with no save of the game has no copy; none is not an error.
Outcome back_up_game(uint64_t title_id);

// "Back up the saves" of the storage screen: back_up_game on a worker, then what it made.
void open(uint64_t title_id, const std::string& name);

// "Restore a saved copy" of the storage screen: the copies of the game on the SD card (back_up_game's),
// the newest first; the one picked replaces the save of its user, the save it replaces kept beside
// them first and put back if anything goes wrong.
void open_restore(uint64_t title_id, const std::string& name);

}   // namespace local_saves_flow
