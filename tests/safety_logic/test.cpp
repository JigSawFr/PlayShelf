// action/safety_logic.cpp: when installing on this console warrants a warning.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/safety_logic.hpp"

#include "check.h"

using namespace safety_logic;

int main()
{
    // Only what is known warns.
    CHECK(!risky({Answer::Unknown, Answer::Unknown}));
    CHECK(!risky({Answer::Yes, Answer::Yes}));      // an emuMMC, Nintendo blocked: safe
    CHECK(!risky({Answer::Yes, Answer::Unknown}));
    CHECK(!risky({Answer::Unknown, Answer::Yes}));
    CHECK(risky({Answer::No, Answer::Yes}));        // the real memory
    CHECK(risky({Answer::Yes, Answer::No}));        // Nintendo reachable
    CHECK(risky({Answer::No, Answer::No}));
    CHECK(risky({Answer::No, Answer::Unknown}));
    CHECK(risky({Answer::Unknown, Answer::No}));

    // Addresses that are blocks.
    CHECK(blocked_address({127, 0, 0, 1}) && blocked_address({0, 0, 0, 0}) && blocked_address({127, 1, 2, 3}));
    CHECK(blocked_address({95, 216, 149, 205}) && blocked_address({163, 172, 141, 219}) && blocked_address({207, 246, 121, 77}));
    CHECK(!blocked_address({95, 216, 149, 206}) && !blocked_address({8, 8, 8, 8}) && !blocked_address({192, 168, 1, 1}));

    // What the names answered.
    const Address nowhere{127, 0, 0, 1}, dns90{207, 246, 121, 77}, nintendo{52, 1, 2, 3};
    CHECK(nintendo_blocked({}) == Answer::Unknown);
    CHECK(nintendo_blocked({{}, {}, {}, {}}) == Answer::Unknown);                   // none resolved: nothing known
    CHECK(nintendo_blocked({{nowhere}, {dns90}, {nowhere, dns90}}) == Answer::Yes);  // all blocked
    CHECK(nintendo_blocked({{nowhere}, {}, {dns90}}) == Answer::Yes);                // one that did not resolve says nothing
    CHECK(nintendo_blocked({{nowhere}, {nintendo}}) == Answer::No);                  // one leads to Nintendo
    CHECK(nintendo_blocked({{nowhere, nintendo}}) == Answer::No);
    CHECK(nintendo_blocked({{}, {nintendo}}) == Answer::No);

    CHECK_DONE("safety logic: a warning only for what is known, blocked addresses and answers passed");
}
