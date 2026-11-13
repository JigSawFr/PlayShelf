// library/popular: popular.json read, what does not hold together left out,
// another schema refused.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/atlas.hpp"
#include "library/popular.hpp"

using namespace library;

int main()
{
    PopularBook b;
    CHECK(!b.from_json("") && !b.from_json("[]") && !b.from_json("{\"games\":{}}") && b.size() == 0);
    CHECK(!b.from_json("{\"schema\":1,\"generated\":\"27/10/2026\",\"games\":{}}"));
    // A file's day read from its first bytes, without parsing it: "" when it is not there as Atlas writes it.
    CHECK(library::atlas_day_at_head(R"({"schema":1,"platform":"switch","generated":"2026-11-13","games":{}})") == "2026-11-13");
    CHECK(library::atlas_day_at_head(R"({"schema":1,"generated":"27/10/2026","games":{}})").empty());
    CHECK(library::atlas_day_at_head(R"({"schema":1,"generated":"2026-11-1","games":{}})").empty());
    CHECK(library::atlas_day_at_head(R"({"schema":1,"games":{"x":{"generated":"2026-11-13"}}})").empty());   // a game's, not the file's
    CHECK(library::atlas_day_at_head(R"({"schema":1,"generated":"2026-11-13)").empty());                       // cut
    CHECK(library::atlas_day_at_head(std::string(600, ' ') + R"("generated":"2026-11-13")").empty());          // too far
    CHECK(library::atlas_day_at_head("").empty());
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-10-27","month":"2026-09","games":{
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
    // The critics' score, Atlas' third field: read when a number, bounded.
    CHECK(b.of(0x0100000000050000).critics == 88 && b.of(0x0100000000060000).critics == 0 &&
          b.of(0x0100000000070000).critics == 100 && b.of(0x010025400AECE000).critics == 0);
    CHECK(b.of(0x010025400AECE000).score == 93 && b.of(0x010025400AECE000).trend == 12);
    CHECK(b.of(0x0100B0C013912000).trend == -40);
    CHECK(b.of(0x0100000000030000).score == 100 && b.of(0x0100000000030000).trend == 999);
    CHECK(b.of(0x0100000000010000).score == 0 && b.of(0x010025400AECE800).score == 0);
    // Where a score stands among the games known: the share of them it beats.
    {
        PopularBook r;
        CHECK(r.share_below(50) == 0);
        CHECK(r.from_json(R"({"schema":1,"generated":"2026-11-11","games":{
            "0100000000010000":[10,0],"0100000000020000":[20,0],"0100000000030000":[30,0],"0100000000040000":[40,0]}})"));
        CHECK(r.share_below(40) == 75 && r.share_below(10) == 0 && r.share_below(30) == 50 && r.share_below(100) == 100);
    }
    // Read again: what was there goes.
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-11-01","games":{}})") && b.size() == 0);
    // Atlas' schema: another one (a field that changed meaning), or none, is
    // refused, the copy PlayShelf had kept by the caller; a field it does
    // not know is skipped.
    CHECK(!b.from_json(R"({"schema":2,"generated":"2026-11-02","games":{"0100000000050000":[40,10]}})"));
    CHECK(!b.from_json(R"({"generated":"2026-11-02","games":{"0100000000050000":[40,10]}})"));
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-11-02","mood":"calm","games":{"0100000000050000":[40,10,88,"x"]}})"));
    CHECK(b.size() == 1 && b.of(0x0100000000050000).critics == 88);
    CHECK_DONE("library_popular");
}
