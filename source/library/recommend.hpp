// recommend — the games of the servers to suggest, from many signals at
// once, each with the reason that weighed most:
//
// - like a game the console plays (PlayInsights' similar.json), the more it
//   was played and the more lately, the more it counts;
// - in a series, or by a developer, of the console's games (games.json);
// - in the genres the console's games have, weighed by their play time;
// - well known, liked by critics and, for a game also on Steam, liked there;
// - out lately;
// - less like the games the player said they do not want (and never those).
//
// The row is then varied: each game picked counts against the next ones of
// its series, by its developer or much like it, so one series does not fill
// it.
//
// Plain C++ for the host tests (tests/library_recommend).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "library/popular.hpp"
#include "library/signals.hpp"

namespace library
{

// A game of the console, and how much it says of its player's taste.
struct Owned
{
    uint64_t id = 0;
    double   weight = 0;   // owned_weight()
    uint32_t hours = 0;    // its play time
};

// A game of the servers the console has not.
struct Candidate
{
    uint64_t                 id = 0;
    uint32_t                 release_date = 0;   // YYYYMMDD, 0 when unknown
    std::vector<std::string> genres;             // Nlib API's, in English
    Renown                   renown;             // PlayInsights' popularity and critics
};

struct Reason
{
    enum class Kind { Like, Series, Developer, Genre, Popular };
    Kind        kind = Kind::Popular;
    uint64_t    because = 0;   // Like: the console's game it is like
    uint32_t    hours = 0;     // Like: that game's play time
    std::string label;         // Series, Developer: its name; Genre: Nlib API's word
};

struct Suggestion
{
    uint64_t id = 0;
    double   score = 0;
    Reason   reason;
};

// How much a game of the console says: its play time (a log of its hours),
// more for one played in the last month, little for one never played.
double owned_weight(uint64_t play_s, uint32_t launches, uint64_t last_played, uint64_t now);

// The candidates worth suggesting, the best first (varied: see above), `max` at most. `genres`:
// the console's games' genres (Nlib API's words), each by how much it weighs
// (their weights summed); `unwanted`: the games the player does not want
// suggested (base title ids); `today`: YYYYMMDD.
std::vector<Suggestion> recommend(const std::vector<Owned>& owned, const std::vector<Candidate>& candidates,
                                  const std::map<std::string, double>& genres, const SimilarBook& similar,
                                  const SignalBook& signals, const std::vector<uint64_t>& unwanted, uint32_t today,
                                  size_t max);

}   // namespace library
