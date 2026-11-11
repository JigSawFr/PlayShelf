// also — the other stores a Switch game is sold on, from also.json (made
// every week by Atlas, https://github.com/JigSawFr/Atlas, from Wikidata):
// a game sheet's "Also on" line. Only which stores, never a link or an id
// to buy by.
//
// Plain C++ for the host tests (tests/library_also).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace library
{

// The stores also.json names, as bits of a game's set.
enum Store : uint8_t { STEAM = 1, PLAYSTATION = 2, XBOX = 4, GOG = 8, EPIC = 16 };

class AlsoBook
{
  public:
    static constexpr size_t MAX_GAMES = 20000;

    // From also.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    // A game's stores, by its base game's title id: a set of Store bits, 0 when none is known.
    uint8_t of(uint64_t base_id) const;
    size_t size() const { return this->games.size(); }
    // The day it was made ("2026-10-27"), to keep the newer of two.
    const std::string& generated() const { return this->day; }

  private:
    std::unordered_map<uint64_t, uint8_t> games;
    std::string                           day;
};

// The stores of a set as their brand names, in a fixed order ("Steam, Xbox"), "" for none.
std::string store_names(uint8_t stores);

}   // namespace library
