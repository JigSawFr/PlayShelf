// library/upcoming: upcoming.json read, what does not hold together left
// out, another schema refused.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/upcoming.hpp"

using namespace library;

int main()
{
    UpcomingBook b;
    CHECK(!b.from_json("") && !b.from_json("[]") && !b.from_json("{\"games\":{}}") && b.size() == 0);
    CHECK(!b.from_json("{\"schema\":1,\"generated\":\"27/10/2026\",\"games\":{}}"));
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-10-28","source":"x","games":{
        "0100000000010000":[20261204,"A game"],
        "0100000000020000":[20270115,"Followed",4200],
        "0100000000030000":[20270115,"Odd hypes","x"],
        "0100000000040000":[20271340,"Bad day"],
        "0100000000050000":[20270115,""],
        "0100000000060000":[20270115,7],
        "0100000000070000":[20270115],
        "0100000000010800":[20270115,"An update"],
        "NOTANID":[20270115,"Nope"],
        "0100000000080000":[20270115,"Huge",999999999999]
    }})"));
    CHECK(b.size() == 4 && b.generated() == "2026-10-28");
    CHECK(b.find(0x0100000000010000) && b.find(0x0100000000010000)->date == 20261204 &&
          b.find(0x0100000000010000)->name == "A game" && b.find(0x0100000000010000)->hypes == 0);
    CHECK(b.find(0x0100000000020000)->hypes == 4200);
    CHECK(b.find(0x0100000000030000)->hypes == 0);
    CHECK(b.find(0x0100000000080000)->hypes == 100000000);
    CHECK(!b.find(0x0100000000040000) && !b.find(0x0100000000999000));
    // Read again: what was there goes.
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-11-01","games":{}})") && b.size() == 0);
    // Another schema, or none, is refused; a field it does not know is skipped.
    CHECK(!b.from_json(R"({"schema":2,"generated":"2026-11-02","games":{"0100000000010000":[20261204,"A"]}})"));
    CHECK(!b.from_json(R"({"generated":"2026-11-02","games":{"0100000000010000":[20261204,"A"]}})"));
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-11-02","mood":"calm","games":{"0100000000010000":[20261204,"A",3,"x"]}})"));
    CHECK(b.size() == 1 && b.find(0x0100000000010000)->hypes == 3);
    CHECK_DONE("library_upcoming");
}
