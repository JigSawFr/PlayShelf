// check_flow — "Check the files" of an installed game: every file of the game, its update and its
// add-ons on the SD card is read back and hashed (install/verify_title), with a bar and Cancel, and
// the user is told what was found: all intact, or which files are missing, of another size,
// unreadable or damaged.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace check_flow
{

// Asks, then checks game `game` (its base game's title id), named `name`, on a worker thread.
void open(uint64_t game, const std::string& name);

}   // namespace check_flow
