// installed_flow — the games on the console for the Library tab: what is
// installed read again (library_flow::reread_installed, the covers of the
// Catalogue tab updated with it) and put together by game
// (library/installed), each game's name and icon from the console
// (core/games), read once a run, and how much each was played (core/play)
// and the room it takes, read with them, with the SD card's room.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "library/installed.hpp"

namespace installed_flow
{

struct Game
{
    library::InstalledGame                      game;
    std::string                                 name;   // the console's, "" when it has none
    std::string                                 version;   // the one it shows ("1.2.0"), "" when none
    std::shared_ptr<const std::vector<uint8_t>> icon;   // a JPEG, nullptr when none
    // How much it was played (core/play), every account together: 0 when
    // never, or the console does not say.
    uint64_t                                    play_s = 0;
    uint32_t                                    launches = 0;
    uint64_t                                    last_played = 0;   // POSIX
    uint64_t                                    size = 0;   // the bytes it takes, 0 when the console does not say
};

// The SD card's room, read with the games.
struct Space
{
    bool     known = false;
    uint64_t total = 0, free = 0;
};
Space space();

// Reads them on a worker thread; `done` on the UI thread, in title id order.
// A read already under way: `done` comes with its end.
void read(std::function<void(std::shared_ptr<const std::vector<Game>>)> done);

// Deletes the game `id` from the console (core_game_delete: its base game,
// update and add-ons; its saves stay) on a worker thread, says so, then
// reads the games again: `done` as read()'s.
void remove(uint64_t id, const std::string& name, const std::function<void(std::shared_ptr<const std::vector<Game>>)>& done);

// "12 h played", "40 min played": a game's play time.
std::string play_text(uint64_t seconds);

// The last read's, nullptr before the first.
std::shared_ptr<const std::vector<Game>> games();

}   // namespace installed_flow
