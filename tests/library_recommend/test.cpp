// library/recommend: the console's taste weighed by play, the signals
// combined, the reason that weighed most, the games with nothing to say for
// them left out.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <string>

#include "check.h"
#include "library/recommend.hpp"

using namespace library;

namespace
{
std::string fmt_id(uint64_t id)
{
    char text[24];
    std::snprintf(text, sizeof text, "\"%016llX\"", (unsigned long long)id);
    return text;
}

// The console's games: a long-played one (P), a never-played one (N).
constexpr uint64_t P = 0x0100000000010000, N = 0x0100000000020000;
// The servers' games.
constexpr uint64_t LIKE_P = 0x0100000000110000, SAME_SERIES = 0x0100000000120000, SAME_GENRE = 0x0100000000130000,
                   FAMOUS = 0x0100000000140000, NOTHING = 0x0100000000150000, LIKE_N = 0x0100000000160000;

void test_weight()
{
    const uint64_t now = 1800000000;
    CHECK(owned_weight(0, 0, 0, now) < 0.5);
    CHECK(owned_weight(100 * 3600, 50, now - 90 * 86400, now) > owned_weight(3600, 2, now - 90 * 86400, now));
    // Played this month: more.
    CHECK(owned_weight(3600, 2, now - 86400, now) > owned_weight(3600, 2, now - 90 * 86400, now));
}

void test_recommend()
{
    SimilarBook similar;
    CHECK(similar.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000010000":[["0100000000110000",90]],
        "0100000000020000":[["0100000000160000",90]],
        "0100000000150000":[["0100000000140000",90]]}})"));
    SignalBook signals;
    CHECK(signals.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000010000":{"series":[["Q1","A saga"]]},
        "0100000000120000":{"series":[["Q1","A saga"]]}}})"));
    const std::vector<Owned> owned = { { P, owned_weight(80 * 3600, 40, 0, 0), 80 }, { N, owned_weight(0, 0, 0, 0), 0 } };
    std::map<std::string, double> genres = { { "Platformer", 5.0 } };
    auto candidate = [](uint64_t id, std::vector<std::string> g, int score, int critics) {
        Candidate c;
        c.id = id;
        c.genres = std::move(g);
        c.renown.score = score;
        c.renown.critics = critics;
        return c;
    };
    const std::vector<Candidate> candidates = {
        candidate(NOTHING, {}, 0, 0),           candidate(FAMOUS, {}, 95, 90),
        candidate(SAME_GENRE, { "Platformer" }, 0, 0), candidate(SAME_SERIES, {}, 0, 0),
        candidate(LIKE_N, {}, 0, 0),            candidate(LIKE_P, {}, 0, 0),
    };
    const auto got = recommend(owned, candidates, genres, similar, signals, {}, 20261029, 10);
    // Nothing for it: left out. Like the much played game first, then its series.
    CHECK(got.size() == 5);
    if (!CHECK(got.size() >= 2)) return;
    CHECK(got[0].id == LIKE_P && got[0].reason.kind == Reason::Kind::Like && got[0].reason.because == P);
    // The played game's hours come with its likeness.
    CHECK(got[0].reason.hours == 80);
    CHECK(got[1].id == SAME_SERIES && got[1].reason.kind == Reason::Kind::Series && got[1].reason.label == "A saga");
    for (const Suggestion& s : got) {
        CHECK(s.id != NOTHING);
        if (s.id == SAME_GENRE) CHECK(s.reason.kind == Reason::Kind::Genre && s.reason.label == "Platformer");
        if (s.id == FAMOUS) CHECK(s.reason.kind == Reason::Kind::Popular);
        // Like a game never played: weighs little.
        if (s.id == LIKE_N) CHECK(s.score < got[0].score / 2);
    }
    // Bounded.
    CHECK(recommend(owned, candidates, genres, similar, signals, {}, 20261029, 2).size() == 2);
    // Nothing known of the console: only the well known.
    const auto cold = recommend({}, candidates, {}, similar, signals, {}, 20261029, 10);
    CHECK(cold.size() == 1 && cold[0].id == FAMOUS);
    // Not wanted: never, and the games much like it fall away.
    const auto unwanted = recommend(owned, candidates, genres, similar, signals, { SAME_SERIES, NOTHING }, 20261029, 10);
    CHECK(unwanted.size() == 3);
    for (const Suggestion& s : unwanted) CHECK(s.id != SAME_SERIES && s.id != FAMOUS);
}

// A row is varied: five games of one series, all scored above three others,
// do not fill its first places.
void test_varied()
{
    // The console plays a game of the series a lot, and a puzzle game.
    constexpr uint64_t SAGA0 = 0x0100000000210000, PUZZLE = 0x0100000000220000;
    std::string games = R"({"schema":1,"generated":"2026-10-29","games":{"0100000000210000":{"series":[["Q7","Lantern Saga"]]},)";
    std::vector<Candidate> candidates;
    for (int k = 0; k < 5; k++) {
        const uint64_t id = 0x0100000000300000 + ((uint64_t)k << 16);
        games += fmt_id(id) + R"(:{"series":[["Q7","Lantern Saga"]]},)";
        Candidate c;
        c.id = id;
        c.renown.score = 90;
        candidates.push_back(c);
    }
    for (int k = 0; k < 3; k++) {
        Candidate c;
        c.id = 0x0100000000400000 + ((uint64_t)k << 16);
        c.genres = { "Puzzle" };
        c.renown.score = 40;
        candidates.push_back(c);
    }
    games.back() = '}';
    games += "}";
    SignalBook signals;
    CHECK(signals.from_json(games));
    SimilarBook similar;
    CHECK(similar.from_json(R"({"schema":1,"generated":"2026-10-29","games":{}})"));
    const std::vector<Owned> owned = { { SAGA0, owned_weight(60 * 3600, 30, 0, 0), 60 }, { PUZZLE, owned_weight(10 * 3600, 5, 0, 0), 10 } };
    const auto row = recommend(owned, candidates, { { "Puzzle", 2.0 } }, similar, signals, {}, 20261029, 8);
    CHECK(row.size() == 8);
    if (!CHECK(row.size() >= 4)) return;
    auto in_saga = [](uint64_t id) { return id >= 0x0100000000300000 && id < 0x0100000000400000; };
    // The best of the series first, then the others before the series' rest.
    CHECK(in_saga(row[0].id));
    int saga_in_first_four = 0;
    for (size_t i = 0; i < 4; i++) saga_in_first_four += in_saga(row[i].id);
    CHECK(saga_in_first_four <= 2);
}
}   // namespace

int main()
{
    test_weight();
    test_recommend();
    test_varied();
    CHECK_DONE("library_recommend");
}
