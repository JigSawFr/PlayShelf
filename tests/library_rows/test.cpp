// library/rows: the rules of Home's Atlas rows: Tonight's pick, Popular in
// your language, Missing from your series.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/rows.hpp"

using namespace library;

namespace
{
constexpr uint64_t OWNED = 0x0100000000010000, SAME_A = 0x0100000000020000, SAME_B = 0x0100000000030000,
                   OTHER = 0x0100000000040000, EDITION = 0x0100000000060000;

void test_tonight()
{
    Renown good;
    good.score = 60, good.critics = 80;   // quality 0.3 + 0.24 = 0.54
    CHECK(tonight_quality(0, 0, good, 0) > 0.53 && tonight_quality(0, 0, good, 0) < 0.55);
    CHECK(tonight_quality(3600, 2, good, 0) > 0);                      // hardly played
    CHECK(tonight_quality(2 * 3600, 0, good, 0) == 0);                 // two hours: played
    CHECK(tonight_quality(0, 3, good, 0) == 0);                        // started three times: played
    Renown unknown;
    unknown.score = 19, unknown.critics = 100;                         // too little known, however good
    CHECK(tonight_quality(0, 0, unknown, 100) == 0);
    Renown middling;
    middling.score = 30, middling.critics = 40;                         // 0.15 + 0.12 = 0.27: not good enough
    CHECK(tonight_quality(0, 0, middling, 0) == 0);
    CHECK(tonight_quality(0, 0, middling, 100) > 0.46 && tonight_quality(0, 0, middling, 100) < 0.48);   // + 0.2 = 0.47
    middling.critics = 30;                                              // 0.15 + 0.09 + 0.2 = 0.44: just short
    CHECK(tonight_quality(0, 0, middling, 100) == 0);
}

void test_language()
{
    ReachBook reach;
    CHECK(reach.from_json(R"({"schema":1,"generated":"2026-11-12","languages":["en","fr","ja"],"games":{
        "0100000000010000":[50,29,21],"0100000000020000":[30,30,40],"0100000000030000":[100,0,0]}})"));
    CHECK(!read_in_language(reach, OWNED, "fr"));       // 29: under the threshold
    CHECK(read_in_language(reach, SAME_A, "fr") && read_in_language(reach, SAME_A, "ja"));   // 30 and 40
    CHECK(!read_in_language(reach, SAME_B, "en"));      // English: never
    CHECK(!read_in_language(reach, SAME_A, ""));        // no language: never
    CHECK(!read_in_language(reach, OTHER, "fr"));       // a game the file does not name
}

void test_series()
{
    SignalBook signals;
    CHECK(signals.from_json(R"({"schema":1,"generated":"2026-11-12","games":{
        "0100000000010000":{"name":"Owned","series":[["Q1","A saga"],["Q2","Another"]]},
        "0100000000020000":{"name":"Same A","series":[["Q1","A saga"]]},
        "0100000000030000":{"name":"Same B","series":[["Q2","Another"]]},
        "0100000000040000":{"name":"Other","series":[["Q9","Elsewhere"]]},
        "0100000000050000":{"series":[["Q1","A saga"]]},
        "0100000000060000":{"name":"Edition","series":[["Q1","A saga"]]}}})"));
    // The console has Owned, played much (weight 3), in two series.
    const std::vector<Owned> owned = { { OWNED, 3.0, 40 } };
    const Series mine = series_of(owned, signals);
    CHECK(mine.size() == 2 && mine.at("Q1").first == 3.0 && mine.at("Q1").second == "A saga" && mine.at("Q2").second == "Another");
    // A second game of the first series, played less: the weight stays the most played's.
    const std::vector<Owned> two = { { OWNED, 3.0, 40 }, { SAME_A, 1.0, 5 } };
    CHECK(series_of(two, signals).at("Q1").first == 3.0);
    CHECK(series_of({}, signals).empty());

    auto renown = [](uint64_t id) { return id == SAME_B ? 90 : id == SAME_A ? 10 : 0; };
    auto none = [](uint64_t) { return false; };
    // The caller rules out the console's own games and the editions.
    const auto all = series_gaps(signals, mine, [](uint64_t id) { return id == OWNED || id == EDITION; }, renown, 10);
    // Same A and Same B are the console's series' games: the same weight, so the better known first;
    // Other is of no series of the console's, the nameless one has no name.
    CHECK(all.size() == 2 && all[0].id == SAME_B && all[0].name == "Same B" && all[0].series == "Another");
    CHECK(all[1].id == SAME_A && all[1].series == "A saga");
    CHECK(series_gaps(signals, mine, none, renown, 1).size() == 1);           // max
    CHECK(series_gaps(signals, mine, none, renown, 0).empty());
    CHECK(series_gaps(signals, Series{}, none, renown, 10).empty());         // the console has no series
    // A series with no name says nothing.
    Series unnamed = { { "Q1", { 3.0, "" } } };
    CHECK(series_gaps(signals, unnamed, [](uint64_t id) { return id == OWNED; }, renown, 10).empty());
    // Equal weights and renown: by title id.
    auto flat = [](uint64_t) { return 0; };
    const auto ties = series_gaps(signals, mine, [](uint64_t id) { return id == OWNED || id == EDITION; }, flat, 10);
    CHECK(ties.size() == 2 && ties[0].id == SAME_A && ties[1].id == SAME_B);
}
}   // namespace

int main()
{
    test_tonight();
    test_language();
    test_series();
    CHECK_DONE("library_rows");
}
