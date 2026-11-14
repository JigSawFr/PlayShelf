// signals — what Atlas (https://github.com/JigSawFr/Atlas)
// knows of each game beyond its popularity, for the recommendations
// (library/recommend) and a game's page: its name (titledb's, for a game no
// server or title database names), its traits from Wikidata (series,
// developers, publishers, genres, modes, each an id and an English label),
// its Steam audience and its players' rating on IGDB (games.json), the games
// most like it (similar.json) and the game an edition is one of
// (editions.json).
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
    std::string id;      // Wikidata's ("Q123"), IGDB's for a theme ("17")
    std::string label;   // English, "" when Wikidata has none
};

struct SteamAudience
{
    uint32_t app = 0;   // 0: not on Steam (or not known)
    uint64_t positive = 0, negative = 0;
    uint64_t owners = 0;   // the lower bound SteamSpy gives
    uint32_t ccu = 0;      // players at once when last seen
};

// IGDB's players' rating: 0..100, and how many rated it (0: not known).
struct PlayersRating
{
    uint32_t rating = 0;
    uint64_t count = 0;
};

struct GameSignals
{
    std::string        name;   // titledb's (US, else GB, else JP), "" when not told
    std::vector<Trait> series, developers, publishers, genres, modes;
    std::vector<Trait> themes;   // IGDB's ("Survival"), once Atlas has its keys
    SteamAudience      steam;
    PlayersRating      igdb;
    std::string        igdb_art;   // the name of IGDB's wide picture of it ("ar1abc"), "" when none
    std::string        igdb_cover; // and of its cover
};

// Reviews or ratings below which a source's say is left out of
// players_rating() (a handful of voices is not an opinion).
constexpr uint64_t STEAM_TRUSTED = 50;
constexpr uint64_t IGDB_TRUSTED = 20;

// A wide picture of a game, asked of wsrv.nl (an image cache and resize service in
// the European Union) which asks IGDB's image service: PlayShelf never contacts
// IGDB itself. `w` × `h` at most, a JPEG; "" when the game has no art.
std::string art_url(const GameSignals& game, int w, int h);

// A game's cover, a square of `size` at most (the middle of IGDB's portrait one), asked of wsrv.nl
// the same way; "" when the game has none.
std::string cover_url(const GameSignals& game, int size);

// How well the players rate a game, 0..100: Steam's share of positive
// reviews and IGDB's rating, the two averaged when both are known; 0 when
// neither is (or too few voted).
int players_rating(const GameSignals& game);

class SignalBook
{
  public:
    static constexpr size_t MAX_GAMES = 20000;
    static constexpr size_t MAX_TRAITS = 12;   // of a kind
    static constexpr size_t MAX_NAME = 256;    // bytes: a longer name is not one

    // From games.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    const GameSignals* find(uint64_t base_id) const;
    // Every game, to search them by a trait.
    const std::unordered_map<uint64_t, GameSignals>& all() const { return this->games; }
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

// A Deluxe, Complete or GOTY edition, or a "+ DLC" bundle, and the game it
// is one of (editions.json): a game and its editions are one game to suggest.
class EditionBook
{
  public:
    static constexpr size_t MAX_GAMES = 20000;

    // From editions.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    // The game `id` is an edition of; `id` itself when it is none.
    uint64_t game_of(uint64_t id) const;
    // The editions known of `game`, by title id.
    std::vector<uint64_t> editions_of(uint64_t game) const;
    size_t size() const { return this->to_game.size(); }
    const std::string& generated() const { return this->day; }

  private:
    std::unordered_map<uint64_t, uint64_t>              to_game;
    std::unordered_map<uint64_t, std::vector<uint64_t>> of_game;
    std::string                                         day;
};

// Whether `book` names one of these games, known only by their title id
// (a list of them to show again once it came).
bool names_any(const SignalBook& book, const std::vector<uint64_t>& unnamed);

// The series and developers Atlas gives a game, one per line, for a search to also match ("" when none).
std::string related_names(const GameSignals& game);

// Whether two games share a series (a Wikidata id, so a name's spelling does not matter).
bool same_series(const GameSignals& a, const GameSignals& b);

// A base game's title id from 16 hex digits; 0 for anything else.
uint64_t base_id_of(const std::string& text);
// Whether a day reads as YYYY-MM-DD.
bool is_day(const std::string& text);

}   // namespace library
