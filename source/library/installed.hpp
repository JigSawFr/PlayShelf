// installed — the games on the console, as the Library tab shows them: what
// ncm lists (each title id, base game, update or add-on, with its newest
// version) put together by game, and the game of a catalogue that is one of
// them (what it has beyond: library::state_of).
//
// Plain C++ for the host tests (tests/library_installed).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "library/titles.hpp"

namespace library
{

struct InstalledGame
{
    uint64_t              id = 0;          // the base game's title id
    bool                  base = false;    // the base game itself is there (else an update or add-ons only: a game card's)
    uint32_t              version = 0;     // the base game's
    uint32_t              update = 0;      // the update's, 0: none
    std::vector<uint64_t> dlcs;            // the add-ons', in order
};

// The console's titles (title id -> version, install::installed_versions)
// by game, in title id order.
std::vector<InstalledGame> installed_games(const std::map<uint64_t, uint32_t>& installed);

// A game's version as the console runs it: its update's, else its own.
inline uint32_t running_version(const InstalledGame& g)
{
    return g.update ? g.update : g.version;
}

// The game of `titles` that is `id` (a base game's title id); -1 for none.
long title_of(const std::vector<Title>& titles, uint64_t id);

}   // namespace library
