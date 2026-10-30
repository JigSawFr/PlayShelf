// library/signals: games.json's traits, Steam audience and IGDB players'
// rating, similar.json's rows, editions.json's games, what does not hold
// together left out.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <fstream>
#include <sstream>
#include <string>

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

void test_igdb()
{
    SignalBook b;
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000010000":{"igdb_rating":[81.6,1240]},
        "0100000000020000":{"igdb_rating":[140,10]},
        "0100000000030000":{"igdb_rating":["80",10]},
        "0100000000040000":{"igdb_rating":[80]}}})"));
    const GameSignals* a = b.find(A);
    if (CHECK(a)) CHECK(a->igdb.rating == 82 && a->igdb.count == 1240);
    // Out of range, not a number, no count: not known.
    for (uint64_t id : { B, C, (uint64_t)0x0100000000040000 })
        if (CHECK(b.find(id))) CHECK(b.find(id)->igdb.rating == 0 && b.find(id)->igdb.count == 0);
}

// titledb's name, which Atlas' games.json gives each game: kept as told;
// none, not a string or too long: not known.
void test_names()
{
    SignalBook b;
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-10-30","games":{
        "0100000000010000":{"name":"Ember Vale™: Deluxe","series":[["Q1","Ember"]]},
        "0100000000020000":{"name":42},
        "0100000000030000":{"name":")" + std::string(SignalBook::MAX_NAME + 1, 'x') + R"("},
        "0100000000040000":{}}})"));
    if (CHECK(b.find(A))) CHECK(b.find(A)->name == "Ember Vale™: Deluxe" && b.find(A)->series.size() == 1);
    for (uint64_t id : { B, C, (uint64_t)0x0100000000040000 })
        if (CHECK(b.find(id))) CHECK(b.find(id)->name.empty());
}

void test_editions()
{
    constexpr uint64_t DELUXE = 0x0100000000110000, GOTY = 0x0100000000120000, BUNDLE = 0x0100000000130000;
    EditionBook e;
    CHECK(!e.from_json(R"({"schema":2,"generated":"2026-10-29","games":{}})"));
    CHECK(e.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000110000":"0100000000010000",
        "0100000000120000":"0100000000010000",
        "0100000000130000":"0100000000110000",
        "0100000000020000":"0100000000020000",
        "0100000000030000":5,
        "zz":"0100000000010000"}})"));
    CHECK(e.size() == 3 && e.generated() == "2026-10-29");
    CHECK(e.game_of(DELUXE) == A && e.game_of(GOTY) == A && e.game_of(A) == A && e.game_of(B) == B);
    // An edition of an edition: of its game.
    CHECK(e.game_of(BUNDLE) == A);
    const std::vector<uint64_t> of_a = e.editions_of(A);
    CHECK(of_a.size() == 3 && of_a[0] == DELUXE && of_a[1] == GOTY && of_a[2] == BUNDLE);
    CHECK(e.editions_of(B).empty() && e.editions_of(DELUXE).empty());
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
    // Another schema, or none, refused; a field it does not know skipped.
    SimilarBook other;
    CHECK(!other.from_json(R"({"schema":2,"generated":"2026-10-29","games":{"0100000000010000":[["0100000000020000",86]]}})"));
    CHECK(!other.from_json(R"({"generated":"2026-10-29","games":{"0100000000010000":[["0100000000020000",86]]}})"));
    CHECK(other.size() == 0);
    CHECK(other.from_json(R"({"schema":1,"generated":"2026-10-29","by":"traits","games":{"0100000000010000":[["0100000000020000",86,"x"]]}})"));
    CHECK(other.between(A, B) == 86);
}
// Whether a list of games known by their title id has one the book names.
void test_names_any()
{
    SignalBook b;
    CHECK(b.from_json(R"({"schema":1,"generated":"2026-10-30","games":{
        "0100000000010000":{"name":"Ember Vale"},"0100000000020000":{"series":[["Q1","A saga"]]}}})"));
    CHECK(names_any(b, { B, A }));
    CHECK(!names_any(b, { B }) && !names_any(b, {}) && !names_any(SignalBook{}, { A }));
}

// The copy PlayShelf ships with (resources/data/games.json, made by
// tools/atlas_copies.py): read, and with titledb's names.
void test_shipped()
{
    std::ifstream in("resources/data/games.json", std::ios::binary);
    if (!CHECK(in)) return;
    std::stringstream text;
    text << in.rdbuf();
    SignalBook b;
    if (!CHECK(b.from_json(text.str()))) return;
    // Well known games, named.
    CHECK(b.size() > 1000);
    for (uint64_t id : { A, (uint64_t)0x01006A800016E000, (uint64_t)0x0100152000022000 })
        CHECK(b.find(id) && !b.find(id)->name.empty());
}
}   // namespace

int main()
{
    test_games();
    test_similar();
    test_igdb();
    test_names();
    test_editions();
    test_names_any();
    test_shipped();
    CHECK(base_id_of("0100000000010000") == A && !base_id_of("0100000000010800") && !base_id_of("x"));
    CHECK_DONE("library_signals");
}
