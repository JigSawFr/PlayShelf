// upcoming — the games still to come out and how many people follow each,
// from upcoming.json (made every week by Atlas,
// https://github.com/JigSawFr/Atlas, from the eShop's dates and IGDB's
// follows): Home's "Coming soon" row, the games titledb lacks and the
// order of the others.
//
// Plain C++ for the host tests (tests/library_upcoming).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace library
{

struct Upcoming
{
    uint32_t    date = 0;    // YYYYMMDD
    std::string name;
    uint32_t    hypes = 0;   // people following it on IGDB; 0: unknown
};

class UpcomingBook
{
  public:
    static constexpr size_t MAX_GAMES = 5000;

    // From upcoming.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    // A game's, by its base game's title id; nullptr when it is not coming.
    const Upcoming* find(uint64_t base_id) const;
    const std::unordered_map<uint64_t, Upcoming>& all() const { return this->games; }
    size_t size() const { return this->games.size(); }
    // The day it was made ("2026-10-27"), to keep the newer of two.
    const std::string& generated() const { return this->day; }

  private:
    std::unordered_map<uint64_t, Upcoming> games;
    std::string                            day;
};

}   // namespace library
