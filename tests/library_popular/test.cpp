// library/popular: popular.json read, what does not hold together left out.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/popular.hpp"

using namespace library;

int main()
{
    PopularBook b;
    CHECK(!b.from_json("") && !b.from_json("[]") && !b.from_json("{\"games\":{}}") && b.size() == 0);
    CHECK(!b.from_json("{\"generated\":\"27/10/2026\",\"games\":{}}"));
    CHECK(b.from_json(R"({"generated":"2026-10-27","month":"2026-09","games":{
        "010025400AECE000":[93,12],
        "0100b0c013912000":[71,-40],
        "010025400AECE800":[50,0],
        "010025400AECF000":[50,0],
        "NOTANID":[50,0],
        "0100000000010000":["50",0],
        "0100000000020000":[0,5],
        "0100000000030000":[250,5000],
        "0100000000040000":[7],
        "0100000000050000":[40,10,88],
        "0100000000060000":[40,10,"88"],
        "0100000000070000":[40,10,250]
    }})"));
    // Updates' and add-ons' ids, malformed entries and scores of 0 left out;
    // the rest bounded.
    CHECK(b.size() == 6 && b.generated() == "2026-10-27");
    // The critics' score, PlayInsights' third field: read when a number, bounded.
    CHECK(b.of(0x0100000000050000).critics == 88 && b.of(0x0100000000060000).critics == 0 &&
          b.of(0x0100000000070000).critics == 100 && b.of(0x010025400AECE000).critics == 0);
    CHECK(b.of(0x010025400AECE000).score == 93 && b.of(0x010025400AECE000).trend == 12);
    CHECK(b.of(0x0100B0C013912000).trend == -40);
    CHECK(b.of(0x0100000000030000).score == 100 && b.of(0x0100000000030000).trend == 999);
    CHECK(b.of(0x0100000000010000).score == 0 && b.of(0x010025400AECE800).score == 0);
    // Read again: what was there goes.
    CHECK(b.from_json(R"({"generated":"2026-11-01","games":{}})") && b.size() == 0);
    CHECK_DONE("library_popular");
}
