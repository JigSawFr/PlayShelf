// safety_logic — whether installing on this console is risky, from what the
// console answered: does it run from an emuMMC (a copy of the internal memory
// on the SD card) or from the real memory, and do the names of Nintendo's
// servers lead to Nintendo or nowhere (90DNS or another block). Plain C++ so
// the host tests can run it (tests/safety_logic); the console's own answers
// come from safety_probe.cpp.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace safety_logic
{

enum class Answer { Unknown, Yes, No };

struct Probe
{
    Answer emummc = Answer::Unknown;            // Yes: it runs from an emuMMC
    Answer nintendo_blocked = Answer::Unknown;  // Yes: the names lead nowhere or to a blocking server
};

// A warning is due when the console is known to run from its internal memory,
// or to reach Nintendo's servers. What is not known never warns: a console
// that cannot answer (a PC, no network) is not told it is at risk.
bool risky(const Probe& probe);

using Address = std::array<uint8_t, 4>;

// An address a block answers with: nowhere (0.x.x.x, 127.x.x.x), or the
// server 90DNS answers the console's connection test with (its addresses are
// written here as they were published: a change there shows as "reachable",
// a warning too many, never as safe).
bool blocked_address(const Address& address);

// What the names answered, one list of addresses per name (empty: the name
// did not resolve). Names that did not resolve say nothing (the console may be
// offline); when none resolved nothing is known; otherwise Yes when every
// address is blocked, No when one is not.
Answer nintendo_blocked(const std::vector<std::vector<Address>>& answers);

}   // namespace safety_logic
