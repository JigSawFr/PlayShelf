// library/seen: the games a server had from its first reading, those seen
// later dated, the dates let go after MAX_DAYS, written and read back; the
// days between two dates; the wishlist's ids; a newer schema's book.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/seen.hpp"

using namespace library;

namespace
{
constexpr uint64_t A = 0x0100000000010000, B = 0x0100000000020000, C = 0x0100000000030000;

void test_book()
{
    SeenBook b;
    CHECK(!b.note("", { A }, 20261001) && !b.note("s1", { A }, 20261301));
    // The first reading: none dated.
    CHECK(b.note("s1", { A, B }, 20261001) && b.first_seen("s1", A) == 0 && b.first_seen("s1", B) == 0);
    // Read again with nothing new: unchanged. C comes: dated.
    CHECK(!b.note("s1", { A, B }, 20261002));
    CHECK(b.note("s1", { A, B, C }, 20261020) && b.first_seen("s1", C) == 20261020);
    // Gone and back: still the first day it was seen.
    CHECK(!b.note("s1", { A }, 20261021) && !b.note("s1", { A, C }, 20261022) && b.first_seen("s1", C) == 20261020);
    // Another server: its own first reading.
    CHECK(b.note("s2", { C }, 20261022) && b.first_seen("s2", C) == 0);
    // Written and read back.
    SeenBook again;
    CHECK(again.from_json(b.to_json()) && again.first_seen("s1", C) == 20261020 && !again.note("s1", { A, B, C }, 20261023));
    // MAX_DAYS later: only known.
    CHECK(again.note("s1", { A }, 20270201) && again.first_seen("s1", C) == 0 && !again.note("s1", { C }, 20270202));
    CHECK(again.forget("s2") && !again.forget("s2"));
    CHECK(!again.from_json("{}") && !again.from_json("nope"));
    CHECK(again.from_json(R"({"schema":1,"servers":{"x":{"known":"0100000000010000,ZZ","dated":{"0100000000010000":20261001,"0100000000020000":20261001,"0100000000010000x":1}}}})"));
    CHECK(again.first_seen("x", A) == 20261001 && again.first_seen("x", B) == 0);
}

// A newer schema's book: not read, and newer() tells the app to leave the
// file alone (action/insights). An unknown key is skipped.
void test_schema()
{
    SeenBook b;
    const std::string newer = R"({"schema":2,"servers":{"x":{"known":"0100000000010000"}}})";
    CHECK(SeenBook::newer(newer) && !b.from_json(newer) && b.first_seen("x", A) == 0);
    CHECK(!SeenBook::newer(b.to_json()) && !SeenBook::newer("nope") && !SeenBook::newer(R"({"servers":{}})"));
    CHECK(!SeenBook::newer(R"({"schema":-3})") && !SeenBook::newer(R"({"schema":"2"})"));
    CHECK(b.from_json(R"({"schema":1,"shelf":"top","servers":{"x":{"known":"0100000000010000","pinned":true}}})"));
    CHECK(!b.note("x", { A }, 20261001));   // known already
}

void test_days()
{
    CHECK(days_between(20261001, 20261031) == 30 && days_between(20240228, 20240301) == 2 && days_between(20230228, 20230301) == 1);
    CHECK(days_between(20261231, 20270101) == 1 && days_between(20270101, 20261231) == -1 && days_between(0, 20261231) == 0);
}

void test_ids()
{
    CHECK(ids_text({ A, C }) == "schema=1\n0100000000010000\n0100000000030000\n");
    const auto ids = ids_of("0100000000010000\r\nbad\n0100000000010000\n0100000000030000\n0100000000020000", 2);
    CHECK(ids.size() == 2 && ids[0] == A && ids[1] == C);
    // The file's schema line: written, read past; none (an older file) is 1;
    // a newer one gives nothing, and ids_newer() keeps it from being saved over.
    CHECK(ids_text({ A }) == "schema=1\n0100000000010000\n" && ids_of(ids_text({ A, C }), 10).size() == 2);
    CHECK(!ids_newer(ids_text({})) && !ids_newer("0100000000010000\n") && !ids_newer(""));
    CHECK(ids_newer("schema=2\n0100000000010000\n") && ids_of("schema=2\n0100000000010000\n", 10).empty());
}
}   // namespace

int main()
{
    test_book();
    test_schema();
    test_days();
    test_ids();
    CHECK_DONE("library_seen");
}
