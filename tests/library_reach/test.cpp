// library/reach: reach.json read, what does not hold together left out,
// another schema refused; a console's locale as a language.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/reach.hpp"

using namespace library;

int main()
{
    ReachBook b;
    CHECK(!b.from_json("") && !b.from_json("[]") && !b.from_json("{\"games\":{}}") && b.size() == 0);
    CHECK(!b.from_json(R"({"schema":1,"generated":"2026-11-10","games":{}})"));   // no languages
    CHECK(!b.from_json(R"({"schema":1,"generated":"10/11/2026","languages":["en"],"games":{}})"));
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-11-10","source":"x","languages":["en","fr","ja"],"games":{
        "0100000000010000":[25,5,70],
        "0100000000020000":[100,0],
        "0100000000030000":[250,-4,"x"],
        "0100000000040000":"no",
        "0100000000010800":[1,1,1],
        "NOTANID":[1,1,1]
    }})"));
    CHECK(b.size() == 3 && b.generated() == "2026-11-10" && b.languages().size() == 3 && b.languages()[2] == "ja");
    CHECK(b.share(0x0100000000010000, "ja") == 70 && b.share(0x0100000000010000, "fr") == 5);
    // A short list: what is missing is 0; shares bounded; a text is 0.
    CHECK(b.share(0x0100000000020000, "en") == 100 && b.share(0x0100000000020000, "ja") == 0);
    CHECK(b.share(0x0100000000030000, "en") == 100 && b.share(0x0100000000030000, "fr") == 0 && b.share(0x0100000000030000, "ja") == 0);
    // Unknown game, language or empty one: 0.
    CHECK(b.share(0x0100000000999000, "fr") == 0 && b.share(0x0100000000010000, "de") == 0 && b.share(0x0100000000010000, "") == 0);
    // Another schema, or none, is refused; a field it does not know is skipped.
    CHECK(!b.from_json(R"({"schema":2,"generated":"2026-11-11","languages":["en"],"games":{}})"));
    CHECK(!b.from_json(R"({"generated":"2026-11-11","languages":["en"],"games":{}})"));
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-11-11","mood":"calm","languages":["en","zh"],"games":{"0100000000010000":[1,2,3]}})"));
    CHECK(b.share(0x0100000000010000, "zh") == 2);
    // The console's locale as the language.
    CHECK(language_of("fr") == "fr" && language_of("pt-BR") == "pt" && language_of("zh-Hant") == "zh" &&
          language_of("en-US") == "en" && language_of("") == "");
    CHECK_DONE("library_reach");
}
