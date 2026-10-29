// library/signals: games.json's traits and Steam audience, similar.json's
// rows, what does not hold together left out.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/signals.hpp"

using namespace library;

namespace
{
constexpr uint64_t A = 0x0100000000010000, B = 0x0100000000020000, C = 0x0100000000030000;

void test_games()
{
    SignalBook b;
    CHECK(!b.from_json("") && !b.from_json("{\"schema\":2,\"generated\":\"2026-10-29\",\"games\":{}}"));
    CHECK(!b.from_json("{\"schema\":1,\"generated\":\"soon\",\"games\":{}}"));
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000010000":{"series":[["Q1","A saga"],["bad","x"],["Q2"]],"developers":[["Q3","A studio\nline"]],
                            "steam":{"app":504230,"positive":100,"negative":5,"owners":20000,"ccu":-3}},
        "0100000000020000":{"genres":[["Q9",""]],"steam":{"positive":10}},
        "0100000000010800":{"series":[["Q1","A saga"]]},
        "0100000000030000":"nope"
    }})"));
    CHECK(b.size() == 2 && b.generated() == "2026-10-29");
    const GameSignals* a = b.find(A);
    if (!CHECK(a)) return;
    // Malformed traits left out; a label never holds a line break.
    CHECK(a->series.size() == 1 && a->series[0].id == "Q1" && a->series[0].label == "A saga");
    CHECK(a->developers.size() == 1 && a->developers[0].label == "A studio line");
    CHECK(a->steam.app == 504230 && a->steam.positive == 100 && a->steam.owners == 20000 && a->steam.ccu == 0);
    // No app: no Steam audience.
    CHECK(b.find(B) && b.find(B)->steam.app == 0 && b.find(B)->steam.positive == 0 && b.find(B)->genres.size() == 1);
    CHECK(!b.find(C) && !b.find(0x0100000000010800));
}

void test_similar()
{
    SimilarBook s;
    CHECK(s.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000010000":[["0100000000030000",40],["0100000000020000",86],["0100000000010000",99],["zz",50],["0100000000040000",0]],
        "0100000000020000":[["0100000000030000",250]],
        "0100000000050000":[]
    }})"));
    // Itself, malformed and zero rows left out; the most alike first; bounded.
    const SimilarBook::Row* a = s.find(A);
    CHECK(a && a->size() == 2 && (*a)[0].first == B && (*a)[0].second == 86 && (*a)[1].first == C);
    CHECK(s.find(B) && (*s.find(B))[0].second == 100);
    CHECK(!s.find(0x0100000000050000) && s.size() == 2);
    // Whichever names the other, the best.
    CHECK(s.between(A, B) == 86 && s.between(B, A) == 86 && s.between(C, B) == 100 && s.between(C, A) == 40);
    CHECK(s.between(A, 0x0100000000040000) == 0);
}
}   // namespace

int main()
{
    test_games();
    test_similar();
    CHECK(base_id_of("0100000000010000") == A && !base_id_of("0100000000010800") && !base_id_of("x"));
    CHECK_DONE("library_signals");
}
