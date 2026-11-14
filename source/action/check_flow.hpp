// check_flow — "Check the files" of an installed game: every file of the game, its update and its
// add-ons on the SD card is read back and hashed (install/verify_title), with a bar and Cancel, and
// the user is told what was found: all intact, or which files are missing, of another size,
// unreadable or damaged, and offered the repair when the library has the packages.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "action/library_flow.hpp"

namespace check_flow
{

// Asks, then checks game `game` (its base game's title id), named `name`, on a worker thread. When
// the game is in a library server's catalogue (`data`, its title `index`: insights::found_of), a
// damaged game can be repaired: its damaged files deleted and their packages installed again.
void open(uint64_t game, const std::string& name, std::shared_ptr<const library_flow::Loaded> data = nullptr, size_t index = 0);

}   // namespace check_flow
