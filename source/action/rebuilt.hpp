// rebuilt — the games installed from a package stored compressed and not
// signed by Nintendo (hacBrewPackLZ and the like), by their base game's
// title id, kept in data_dir()/rebuilt.txt. Such a game has its update inside
// it and the official update would break it, so the library does not offer
// one (library::set_rebuilt). A game PlayShelf installs again from an
// official package leaves the list. UI thread only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <map>

namespace rebuilt
{

// Read once, at start-up.
void start();

// What an install wrote: its program NCAs by base game, true for a rebuilt one
// (install::Installer::programs).
void note(const std::map<uint64_t, bool>& programs);

// What the console has now (title id -> version): a game no longer installed
// leaves the list, so one put back by another installer is not taken for a
// rebuilt one. Nothing is dropped from an empty list (the read may have
// failed), nor a game noted since this start (the read may be older).
void prune(const std::map<uint64_t, uint32_t>& installed);

}   // namespace rebuilt
