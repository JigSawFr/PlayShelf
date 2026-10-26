// facts — what is known of a game beyond what its catalogue says, gathered
// as the games show (Nlib API's details: action/nlib_flow) and kept on the
// SD card: its publisher and developer, its date, its genres, how many play
// it, its languages, its age rating, whether it is a demo, its eShop id;
// its name, for a game known only by its title id. The
// pages show them; the Catalogue and the Library sort and filter by them.
//
// Plain C++ for the host tests (tests/library_facts).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace library
{

struct Facts
{
    std::string              name;               // Nlib API's ("" when unknown)
    std::string              publisher, developer;
    uint32_t                 release_date = 0;   // YYYYMMDD, 0 when unknown
    std::vector<std::string> categories;         // the eShop's genres, in English
    std::vector<std::string> languages;          // "en", "fr", "zh"…
    std::vector<std::string> rating;             // its age rating's descriptors, in English
    int                      players = 0;        // the most at once, 0 when unknown
    bool                     demo = false;
    uint64_t                 nsu_id = 0;         // its eShop id, 0 when unknown
};

// The games known so far, by their base game's title id; MAX_FACTS at most
// (a game more is not kept).
class FactsBook
{
  public:
    static constexpr size_t MAX_FACTS = 50000;

    const Facts* find(uint64_t base_id) const;
    // Kept (in place of what was known of it); false when the book is full.
    bool put(uint64_t base_id, Facts facts);
    size_t size() const { return this->games.size(); }
    // The games of `older` this book does not know yet, kept (as room allows).
    void add_missing(const FactsBook& older);

    // As facts.json holds it; read back, the entries that hold together
    // (false: not JSON, or not the book's).
    std::string to_json() const;
    bool        from_json(const std::string& text);

    // Every genre and every publisher known, sorted, each once.
    std::vector<std::string> categories() const;
    std::vector<std::string> publishers() const;

  private:
    std::map<uint64_t, Facts> games;
};

// "20170303" from Nlib API's "2017-03-03" (0 when it is not a date).
uint32_t date_of(const std::string& ymd);

}   // namespace library
