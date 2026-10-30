// library/insights_index: Atlas' index.json read (schema 1, limits,
// bad entries skipped), the short hash, and what to fetch again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/insights_index.hpp"

using namespace library;

namespace
{
void test_read()
{
    InsightsIndex index;
    CHECK(index.from_json(R"({"schema":1,"files":{
        "platforms/switch/popular.json":{"generated":"2026-10-29","bytes":70123,"sha256":"0123456789abcdef"},
        "platforms/switch/prices/FR.json":{"generated":"2026-10-29","bytes":512,"sha256":"fedcba9876543210"},
        "../secret":{"generated":"2026-10-29","bytes":1,"sha256":"0123456789abcdef"},
        "/abs.json":{"generated":"2026-10-29","bytes":1,"sha256":"0123456789abcdef"},
        "upper.json":{"generated":"2026-10-29","bytes":1,"sha256":"0123456789ABCDEF"},
        "short.json":{"generated":"2026-10-29","bytes":1,"sha256":"0123"},
        "noday.json":{"generated":"soon","bytes":1,"sha256":"0123456789abcdef"},
        "minus.json":{"generated":"2026-10-29","bytes":-1,"sha256":"0123456789abcdef"},
        "huge.json":{"generated":"2026-10-29","bytes":999999999999,"sha256":"0123456789abcdef"},
        "odd.json":[1,2,3]}})"));
    CHECK(index.size() == 2);
    const IndexEntry* p = index.find("platforms/switch/popular.json");
    if (CHECK(p != nullptr)) CHECK(p->generated == "2026-10-29" && p->bytes == 70123 && p->sha256 == "0123456789abcdef");
    CHECK(index.find("platforms/switch/prices/FR.json") != nullptr);
    CHECK(index.find("popular.json") == nullptr);
    CHECK(index.find("platforms/switch/games.json") == nullptr);
    // Not one: another schema, no files, not JSON.
    InsightsIndex other;
    CHECK(!other.from_json(R"({"schema":2,"files":{}})"));
    CHECK(!other.from_json(R"({"schema":1})"));
    CHECK(!other.from_json("not json"));
    CHECK(other.size() == 0);
    // An index read again forgets the last one.
    CHECK(index.from_json(R"({"schema":1,"files":{}})") && index.size() == 0);
}

void test_hash()
{
    CHECK(short_hash("abc") == "ba7816bf8f01cfea");
    CHECK(short_hash("").size() == 16);
}

void test_must_fetch()
{
    const std::string cached = "abc";
    InsightsIndex     index;
    CHECK(index.from_json(R"({"schema":1,"files":{
        "platforms/switch/popular.json":{"generated":"2026-10-29","bytes":3,"sha256":"ba7816bf8f01cfea"},
        "platforms/switch/games.json":{"generated":"2026-10-29","bytes":3,"sha256":"0000000000000000"},
        "platforms/switch/similar.json":{"generated":"2026-10-29","bytes":4,"sha256":"ba7816bf8f01cfea"}}})"));
    // The same bytes: kept.
    CHECK(!must_fetch(&index, "platforms/switch/popular.json", cached));
    // Changed, or another size: fetched.
    CHECK(must_fetch(&index, "platforms/switch/games.json", cached));
    CHECK(must_fetch(&index, "platforms/switch/similar.json", cached));
    // No cached copy, a file not listed, no index: fetched, as before the index.
    CHECK(must_fetch(&index, "platforms/switch/popular.json", ""));
    CHECK(must_fetch(&index, "platforms/switch/upcoming.json", cached));
    CHECK(must_fetch(nullptr, "platforms/switch/popular.json", cached));
}
}   // namespace

int main()
{
    test_read();
    test_hash();
    test_must_fetch();
    CHECK_DONE("library_index");
}
