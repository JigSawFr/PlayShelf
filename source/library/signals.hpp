// signals — what PlayInsights (https://github.com/JigSawFr/PlayInsights)
// knows of each game beyond its popularity, for the recommendations
// (library/recommend) and a game's page: its traits from Wikidata (series,
// developers, publishers, genres, modes, each an id and an English label)
// and its Steam audience (games.json), and the games most like it
// (similar.json).
//
// Plain C++ for the host tests (tests/library_signals).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace library
{

struct Trait
{
    std::string id;      // Wikidata's ("Q123")
    std::string label;   // English, "" when Wikidata has none
};

struct SteamAudience
{
    uint32_t app = 0;   // 0: not on Steam (or not known)
    uint64_t positive = 0, negative = 0;
    uint64_t owners = 0;   // the lower bound SteamSpy gives
    uint32_t ccu = 0;      // players at once when last seen
};

struct GameSignals
{
    std::vector<Trait> series, developers, publishers, genres, modes;
    SteamAudience      steam;
};

class SignalBook
{
  public:
    static constexpr size_t MAX_GAMES = 20000;
    static constexpr size_t MAX_TRAITS = 12;   // of a kind

    // From games.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    const GameSignals* find(uint64_t base_id) const;
    size_t size() const { return this->games.size(); }
    const std::string& generated() const { return this->day; }

  private:
    std::unordered_map<uint64_t, GameSignals> games;
    std::string                               day;
};

class SimilarBook
{
  public:
    static constexpr size_t MAX_GAMES = 20000;
    static constexpr size_t MAX_SIMILAR = 24;

    using Row = std::vector<std::pair<uint64_t, int>>;   // a game alike, 1..100

    // From similar.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    // A game's most alike, the most first; nullptr when none is known.
    const Row* find(uint64_t base_id) const;
    // How alike two games are, 0..100, whichever names the other (0: not known).
    int between(uint64_t a, uint64_t b) const;
    size_t size() const { return this->games.size(); }
    const std::string& generated() const { return this->day; }

  private:
    std::unordered_map<uint64_t, Row> games;
    std::string                       day;
};

// A base game's title id from 16 hex digits; 0 for anything else.
uint64_t base_id_of(const std::string& text);
// Whether a day reads as YYYY-MM-DD.
bool is_day(const std::string& text);

}   // namespace library
