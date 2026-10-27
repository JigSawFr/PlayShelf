// popular — how well known each game is and whether it is talked about more
// these days, from popular.json (tools/popular.py makes it from Wikidata and
// Wikipedia's pageviews; docs/contributing/popular.md): Home's "Popular on
// your servers" and "Trending" rows. A score from 0 to 100, and the change
// of its readers last month in percent.
//
// Plain C++ for the host tests (tests/library_popular).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace library
{

struct Renown
{
    int score = 0;   // 0 to 100; 0: unknown
    int trend = 0;   // last month's readers against the two before, in percent
};

class PopularBook
{
  public:
    static constexpr size_t MAX_GAMES = 20000;

    // From popular.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    // A game's, by its base game's title id; a score of 0 when unknown.
    Renown of(uint64_t base_id) const;
    size_t size() const { return this->games.size(); }
    // The day it was made ("2026-10-27"), to keep the newer of two.
    const std::string& generated() const { return this->day; }

  private:
    std::unordered_map<uint64_t, Renown> games;
    std::string                          day;
};

}   // namespace library
