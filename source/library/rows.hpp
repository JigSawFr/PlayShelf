// rows — which games Home's Atlas rows pick, apart from the screens that
// show them: Tonight's pick (the console's games hardly played and well
// rated), Popular in your language (much read in the console's language)
// and Missing from your series (a series the console has games of, the
// games no server has). The thresholds are here, so they are tested.
//
// Plain C++ for the host tests (tests/library_rows).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "library/popular.hpp"
#include "library/reach.hpp"
#include "library/recommend.hpp"
#include "library/signals.hpp"

namespace library
{

// Tonight's pick: played under this long and started fewer times than this,
// of a renown of this much and a quality (quality_of) of this much at least.
constexpr uint64_t TONIGHT_PLAY_S = 2 * 3600;
constexpr uint32_t TONIGHT_LAUNCHES = 3;
constexpr int      TONIGHT_RENOWN = 20;
constexpr double   TONIGHT_QUALITY = 0.45;

// How good a game is for Tonight's pick (library::quality_of), or 0 when the game is not one: played a
// lot, started often, or not known or not good enough.
double tonight_quality(uint64_t play_s, uint32_t launches, const Renown& renown, int players_rating);

// To play together: how a game says several play it on one console, by the modes Atlas gives it
// (Wikidata): on one screen (shared or split screen, hotseat) before co-op, None for the others
// (multiplayer alone may be online only).
enum class Together : uint8_t { None, Coop, SameScreen };
Together together_of(const GameSignals& game);

// Popular in your language: a share of a game's readers of this much (in percent) at least.
constexpr int LOCAL_SHARE = 30;

// Whether a game is much read in `language` (a code such as "fr"): never in an empty one, nor in English,
// which almost every game is read in.
bool read_in_language(const ReachBook& reach, uint64_t base_id, const std::string& language);

// The series of the console's games, by Wikidata id: the weight of the one most played of them and the
// series' name.
using Series = std::map<std::string, std::pair<double, std::string>>;
Series series_of(const std::vector<Owned>& owned, const SignalBook& signals);

struct SeriesGame
{
    uint64_t    id = 0;
    std::string name, series;
};

// Missing from your series: the games Atlas names (with a name, of a series in `mine` with a name) that
// `excluded` does not rule out, the most played game's series first, then the better known game (`renown`),
// then by title id; `max` at most.
std::vector<SeriesGame> series_gaps(const SignalBook& signals, const Series& mine, const std::function<bool(uint64_t)>& excluded,
                                    const std::function<int(uint64_t)>& renown, size_t max);

}   // namespace library
