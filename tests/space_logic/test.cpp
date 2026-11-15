// action/space_logic.cpp: the room missing, and the games to remove for it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/space_logic.hpp"

#include "check.h"

using namespace space_logic;

namespace
{
constexpr uint64_t G = 1ull << 30;

Candidate game(uint64_t id, uint64_t size_g, uint64_t last, bool keep = false)
{
    Candidate c;
    c.id = id;
    c.size = size_g * G;
    c.last_played = last;
    c.keep = keep;
    return c;
}
}   // namespace

int main()
{
    // What is missing: the package and the spare against the room.
    CHECK(shortfall(10 * G, 2 * G) == 0);
    CHECK(shortfall(3 * G, 2 * G) == 0);       // exactly the package and the spare
    CHECK(shortfall(2 * G, 2 * G) == 1 * G);
    CHECK(shortfall(0, 5 * G) == 6 * G);
    CHECK(shortfall(0, UINT64_MAX) == UINT64_MAX);   // no wrap around

    // Nothing missing, nothing to remove.
    CHECK(plan(0, {game(1, 5, 100)}).enough && plan(0, {game(1, 5, 100)}).picks.empty());

    // The oldest first, as few as possible.
    {
        const std::vector<Candidate> g = {game(1, 4, 300), game(2, 6, 100), game(3, 3, 200)};
        Plan p = plan(5 * G, g);
        CHECK(p.enough && p.picks == std::vector<size_t>({1}) && p.freed == 6 * G);   // the oldest one is enough
        p = plan(8 * G, g);
        CHECK(p.enough && p.picks == std::vector<size_t>({1, 2}) && p.freed == 9 * G);
    }
    // A pick the later ones make needless is dropped: the big old one stays when two others suffice... and
    // the small old one goes when the big one after it does the job.
    {
        const std::vector<Candidate> g = {game(1, 1, 100), game(2, 9, 200), game(3, 2, 300)};
        const Plan                   p = plan(8 * G, g);
        CHECK(p.enough && p.picks == std::vector<size_t>({1}) && p.freed == 9 * G);
    }
    // Never played after the played ones; kept and unknown sizes are never suggested.
    {
        std::vector<Candidate> g = {game(1, 5, 0), game(2, 5, 500), game(3, 5, 100, true), game(4, 0, 50)};
        const Plan             p = plan(5 * G, g);
        CHECK(p.enough && p.picks == std::vector<size_t>({1}));
        const Plan all = plan(11 * G, g);   // everything allowed is not enough
        CHECK(!all.enough && all.picks.size() == 2 && all.freed == 10 * G);
    }
    // Equal dates: the larger first.
    {
        const std::vector<Candidate> g = {game(1, 2, 100), game(2, 7, 100)};
        const Plan                   p = plan(6 * G, g);
        CHECK(p.enough && p.picks == std::vector<size_t>({1}));
    }
    CHECK(!plan(1, {}).enough);
    CHECK_DONE("space_logic: the room missing, the games to remove for it passed");
}
