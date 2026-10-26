// installed_flow — the games on the console for the Library tab: what is
// installed read again (library_flow::reread_installed, the covers of the
// Catalogue tab updated with it) and put together by game
// (library/installed), each game's name and icon from the console
// (core/games), read once a run.
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
};

// Reads them on a worker thread; `done` on the UI thread, in title id order.
// A read already under way: `done` comes with its end.
void read(std::function<void(std::shared_ptr<const std::vector<Game>>)> done);

// The last read's, nullptr before the first.
std::shared_ptr<const std::vector<Game>> games();

}   // namespace installed_flow
