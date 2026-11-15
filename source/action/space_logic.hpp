// space_logic — what to remove to make room: when a download stops for lack of space on the SD card,
// the games played least recently that, taken off, free enough. Only a suggestion: nothing is
// removed by it. Plain C++, host-tested (tests/space_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <vector>

namespace space_logic
{

constexpr uint64_t SPARE = 1ull << 30;   // left free besides what a package takes (download::room_ahead's)

// What is missing to write `remaining` bytes and still keep SPARE free; 0 when there is room.
uint64_t shortfall(uint64_t free, uint64_t remaining);

struct Candidate
{
    uint64_t id = 0;
    uint64_t size = 0;          // the bytes it takes; 0 when unknown (never suggested)
    uint64_t last_played = 0;   // POSIX; 0: never played, or the console does not say
    uint64_t play_s = 0;
    bool     keep = false;      // not to suggest (a game of the download queue)
};

struct Plan
{
    std::vector<size_t> picks;   // indexes into the candidates, the least recently played first
    uint64_t            freed = 0;
    bool                enough = false;
};

// The games to remove for `missing` bytes, the fewest that do: the oldest last-played first, those
// never played after the others (they may have just been installed), larger first among equals; then
// any that the others make needless is dropped. `enough` false: every game would not be enough.
Plan plan(uint64_t missing, const std::vector<Candidate>& games);

}   // namespace space_logic
