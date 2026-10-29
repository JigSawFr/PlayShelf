// library/recommend: the console's taste weighed by play, the signals
// combined, the reason that weighed most, the games with nothing to say for
// them left out.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/recommend.hpp"

using namespace library;

namespace
{
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
        "0100000000020000":[["0100000000160000",90]]}})"));
    SignalBook signals;
    CHECK(signals.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000010000":{"series":[["Q1","A saga"]]},
        "0100000000120000":{"series":[["Q1","A saga"]]}}})"));
    const std::vector<Owned> owned = { { P, owned_weight(80 * 3600, 40, 0, 0) }, { N, owned_weight(0, 0, 0, 0) } };
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
    const auto got = recommend(owned, candidates, genres, similar, signals, 20261029, 10);
    // Nothing for it: left out. Like the much played game first, then its series.
    CHECK(got.size() == 5);
    if (!CHECK(got.size() >= 2)) return;
    CHECK(got[0].id == LIKE_P && got[0].reason.kind == Reason::Kind::Like && got[0].reason.because == P);
    CHECK(got[1].id == SAME_SERIES && got[1].reason.kind == Reason::Kind::Series && got[1].reason.label == "A saga");
    for (const Suggestion& s : got) {
        CHECK(s.id != NOTHING);
        if (s.id == SAME_GENRE) CHECK(s.reason.kind == Reason::Kind::Genre && s.reason.label == "Platformer");
        if (s.id == FAMOUS) CHECK(s.reason.kind == Reason::Kind::Popular);
        // Like a game never played: weighs little.
        if (s.id == LIKE_N) CHECK(s.score < got[0].score / 2);
    }
    // Bounded.
    CHECK(recommend(owned, candidates, genres, similar, signals, 20261029, 2).size() == 2);
    // Nothing known of the console: only the well known.
    const auto cold = recommend({}, candidates, {}, similar, signals, 20261029, 10);
    CHECK(cold.size() == 1 && cold[0].id == FAMOUS);
}
}   // namespace

int main()
{
    test_weight();
    test_recommend();
    CHECK_DONE("library_recommend");
}
