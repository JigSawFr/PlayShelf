// library/changes: Atlas' changes.json read for the Switch (the games out
// today, the games new, the trends moved, the sales started and the new
// lowest prices by country), the
// schema rule (absent or another: none; unknown fields skipped), bad
// entries skipped.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <string>

#include "check.h"
#include "library/changes.hpp"

using namespace library;

namespace
{
constexpr uint64_t OUT = 0x0100000000010000, SALE = 0x0100000000020000, LOW = 0x0100000000030000;

void test_read()
{
    ChangeBook book;
    CHECK(book.from_json(R"({"schema":1,"generated":"2026-11-02","source":"Atlas","later":{"x":1},
        "platforms":{
          "steam":{"sales":{"FR":{"started":["900010"]}}},
          "switch":{"new":["0100000000040000"],"trends":[["0100000000010000",10,210]],"comets":[1],
                    "out_today":["0100000000010000","0100000000010800","nothex",7],
                    "sales":{"FR":{"started":["0100000000020000"],"ended":["0100000000030000"],"lowest":["0100000000030000"],"soon":[]},
                             "XX":{"started":["0100000000010000"]},
                             "US":"odd"}}}})"));
    CHECK(book.generated() == "2026-11-02" && book.day_number() == 20261102);
    CHECK(book.out_today(OUT) && !book.out_today(SALE));
    // An update's id, no id, no string: skipped.
    CHECK(!book.out_today(0x0100000000010800));
    CHECK(book.sale_started("FR", SALE) && !book.sale_started("FR", LOW) && !book.sale_started("US", SALE));
    CHECK(book.new_lowest("FR", LOW) && !book.new_lowest("FR", SALE));
    // Not a country of Atlas' prices.
    CHECK(!book.sale_started("XX", OUT));
    // New and trends, in the file's order.
    CHECK(book.new_games().size() == 1 && book.new_games()[0] == 0x0100000000040000);
    CHECK(book.trends().size() == 1 && book.trends()[0].first == OUT && book.trends()[0].second.before == 10 &&
          book.trends()[0].second.now == 210);
    CHECK(book.size() == 5);
}

void test_new_and_trends()
{
    ChangeBook book;
    CHECK(book.from_json(R"({"schema":1,"generated":"2026-11-02","platforms":{"switch":{
        "new":["0100000000040000","0100000000040000","0100000000040800",3,"0100000000050000"],
        "trends":[["0100000000010000",10,210],["0100000000010000",0,500],["0100000000020000",50],
                  ["0100000000030000",-100,1000],["0100000000060000",300,-100],"odd",["zz",1,2],
                  ["0100000000070000",1.5,200],["0100000000080000",0,120,"later"]]}}})"));
    // Each game once, an update's id and malformed entries skipped.
    CHECK(book.new_games().size() == 2 && book.new_games()[1] == 0x0100000000050000);
    // Out of -100..999, not numbers, too short: skipped; a longer entry read.
    CHECK(book.trends().size() == 3);
    CHECK(book.trends()[0].second.now == 210);
    CHECK(book.trends()[1].first == 0x0100000000060000 && book.trends()[1].second.now == -100);
    CHECK(book.trends()[2].first == 0x0100000000080000 && book.trends()[2].second.now == 120);
    // Read again: the last file's lists only.
    CHECK(book.from_json(R"({"schema":1,"generated":"2026-11-03","platforms":{"switch":{"new":"odd","trends":{}}}})"));
    CHECK(book.new_games().empty() && book.trends().empty());
    CHECK(!book.from_json(R"({"schema":2,"generated":"2026-11-04","platforms":{"switch":{"new":["0100000000040000"]}}})"));
    CHECK(book.new_games().empty());
}

void test_schema()
{
    ChangeBook book;
    // A newer schema is no file: the book is left empty.
    CHECK(book.from_json(R"({"schema":1,"generated":"2026-11-02","platforms":{"switch":{"out_today":["0100000000010000"]}}})"));
    CHECK(!book.from_json(R"({"schema":2,"generated":"2026-11-03","platforms":{"switch":{"out_today":["0100000000010000"]}}})"));
    CHECK(book.size() == 0 && book.generated().empty() && !book.out_today(OUT));
    // No schema: not Atlas' schema 1 either.
    CHECK(!book.from_json(R"({"generated":"2026-11-02","platforms":{"switch":{"out_today":["0100000000010000"]}}})"));
    CHECK(book.size() == 0);
    // No day, not JSON, not an object.
    CHECK(!book.from_json(R"({"schema":1,"platforms":{}})"));
    CHECK(!book.from_json("not json"));
    CHECK(!book.from_json("[1]"));
    // Nothing for the Switch: a day with no change.
    CHECK(book.from_json(R"({"schema":1,"generated":"2026-11-02","platforms":{"gog":{}}})"));
    CHECK(book.size() == 0 && book.day_number() == 20261102);
}

void test_limits()
{
    std::string list;
    for (int i = 1; i <= 1500; i++) {
        char id[32];
        std::snprintf(id, sizeof id, "\"0100%08X0000\"", (unsigned)i);
        list += std::string(i > 1 ? "," : "") + id;
    }
    ChangeBook book;
    CHECK(book.from_json(R"({"schema":1,"generated":"2026-11-02","platforms":{"switch":{"out_today":[)" + list + "]}}}"));
    CHECK(book.size() == ChangeBook::MAX_IDS);
}
}   // namespace

int main()
{
    test_read();
    test_new_and_trends();
    test_schema();
    test_limits();
    CHECK_DONE("library_changes");
}
