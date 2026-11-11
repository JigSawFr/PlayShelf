// library/also: also.json read, what does not hold together left out,
// another schema refused; the stores as names.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/also.hpp"

using namespace library;

int main()
{
    AlsoBook b;
    CHECK(!b.from_json("") && !b.from_json("[]") && !b.from_json("{\"games\":{}}") && b.size() == 0);
    CHECK(!b.from_json(R"({"schema":1,"generated":"10/11/2026","games":{}})"));
    CHECK(b.from_json(R"({"schema":1,"platform":"switch","generated":"2026-11-09","source":"x","games":{
        "0100000000010000":{"steam":["900010"],"xbox":["9ZZZZZZZZZZ1"]},
        "0100000000020000":{"gog":["pebble_pilots"],"epic":["a"],"ps":["10900001"],"steam":["1"],"xbox":["X"]},
        "0100000000030000":{"steam":[]},
        "0100000000040000":{"steam":"900010"},
        "0100000000050000":{"nintendo":["x"]},
        "0100000000060000":[],
        "0100000000010800":{"steam":["1"]},
        "NOTANID":{"steam":["1"]}
    }})"));
    CHECK(b.size() == 2 && b.generated() == "2026-11-09");
    CHECK(b.of(0x0100000000010000) == (STEAM | XBOX));
    CHECK(b.of(0x0100000000020000) == (STEAM | PLAYSTATION | XBOX | GOG | EPIC));
    // An empty list, a text, an unknown store: no store; an unknown game too.
    for (uint64_t id : { 0x0100000000030000ull, 0x0100000000040000ull, 0x0100000000050000ull, 0x0100000000999000ull })
        CHECK(b.of(id) == 0);
    CHECK(store_names(STEAM | XBOX) == "Steam, Xbox" && store_names(0).empty());
    CHECK(store_names(STEAM | PLAYSTATION | XBOX | GOG | EPIC) == "Steam, PlayStation, Xbox, GOG, Epic Games");
    // Another schema, or none, is refused; a field it does not know is skipped.
    CHECK(!b.from_json(R"({"schema":2,"generated":"2026-11-11","games":{"0100000000010000":{"steam":["1"]}}})"));
    CHECK(!b.from_json(R"({"generated":"2026-11-11","games":{"0100000000010000":{"steam":["1"]}}})"));
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-11-11","mood":"calm","games":{"0100000000010000":{"steam":["1"],"future":["x"]}}})"));
    CHECK(b.size() == 1 && b.of(0x0100000000010000) == STEAM);
    CHECK_DONE("library_also");
}
