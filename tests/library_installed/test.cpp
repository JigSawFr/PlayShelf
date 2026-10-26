// library/installed.cpp: the console's titles by game (a base game, its
// update, its add-ons; an update or add-ons without their base game), the
// version a game runs, a game found among a catalogue's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/installed.hpp"

using namespace library;

namespace
{
constexpr uint64_t ZELDA = 0x0100000000010000ull, MARIO = 0x0100000000020000ull;

void test_grouping()
{
    const std::map<uint64_t, uint32_t> on_console = {
        { ZELDA, 0 },                      // base
        { ZELDA + 0x800, 131072 },         // its update
        { ZELDA + 0x1001, 0 },             // two add-ons
        { ZELDA + 0x1002, 65536 },
        { MARIO + 0x800, 65536 },          // an update alone (the game on a card)
        { 0, 5 },                          // nothing
    };
    const auto games = installed_games(on_console);
    if (!CHECK(games.size() == 2)) return;
    CHECK(games[0].id == ZELDA && games[0].base && games[0].update == 131072);
    CHECK(games[0].dlcs == (std::vector<uint64_t>{ ZELDA + 0x1001, ZELDA + 0x1002 }));
    CHECK(running_version(games[0]) == 131072);
    CHECK(games[1].id == MARIO && !games[1].base && games[1].update == 65536 && games[1].dlcs.empty());
    CHECK(installed_games({}).empty());
    InstalledGame plain;
    plain.version = 3;
    CHECK(running_version(plain) == 3);
}

void test_title_of()
{
    std::vector<Title> titles(2);
    titles[0].id = MARIO;
    titles[1].id = ZELDA;
    CHECK(title_of(titles, ZELDA) == 1 && title_of(titles, 0x0100000000030000ull) == -1);
}
}   // namespace

int main()
{
    test_grouping();
    test_title_of();
    return CHECK_DONE("installed games: by game, the version run, found in a catalogue passed");
}
