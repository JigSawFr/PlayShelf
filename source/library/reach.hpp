// reach — where a game is read: for each of the best known games, its share
// of the readers of its Wikipedia pages in each language, from reach.json
// (made every day by Atlas, https://github.com/JigSawFr/Atlas, with the
// trends): Home's "Popular in your language" row.
//
// Plain C++ for the host tests (tests/library_reach).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace library
{

class ReachBook
{
  public:
    static constexpr size_t MAX_GAMES = 5000;
    static constexpr size_t MAX_LANGUAGES = 40;

    // From reach.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    // A game's share of the readers in a language, in percent (by its base
    // game's title id and the language's code, "fr"); 0 when unknown.
    int share(uint64_t base_id, const std::string& language) const;
    // The languages the file counts, in its order: read from the file, not
    // assumed (more may come).
    const std::vector<std::string>& languages() const { return this->langs; }
    size_t size() const { return this->games.size(); }
    // The day it was made ("2026-10-27"), to keep the newer of two.
    const std::string& generated() const { return this->day; }

  private:
    std::vector<std::string>                           langs;
    std::unordered_map<uint64_t, std::vector<uint8_t>> games;
    std::string                                        day;
};

// A console's locale ("pt-BR", "zh-Hant", "fr") as the language Atlas
// counts: the part before the dash, in lower case.
std::string language_of(const std::string& locale);

}   // namespace library
