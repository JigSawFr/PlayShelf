// titles — a catalogue as the Library tab shows it: its items gathered by
// game (a base game, its updates newest first, its add-ons), then the games
// a search and a section keep, in the order asked. An item whose title id
// is unknown is a game of its own.
//
// Plain C++ for the host tests (tests/library_titles).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "library/catalog.hpp"

namespace library
{

struct Title
{
    uint64_t            id = 0;   // the base game's title id; 0: unknown
    std::string         name;
    long                base = -1;        // its base game in Catalog::items, -1: none on the server
    std::vector<size_t> updates;          // newest first
    std::vector<size_t> dlcs;             // by name
    uint64_t            size = 0;         // every item's
    uint32_t            release_date = 0; // YYYYMMDD, 0 when unknown
    std::string         icon_url;
    std::vector<std::string> sections;    // the ids of the sections its items are in
};

enum class Sort : uint8_t { Name, Newest, Largest };

std::vector<Title> group_titles(const Catalog& catalog);

// The games whose name (or title id: 8 hex digits at least) holds `query` (case and the
// Latin accents aside; "" keeps every one) and that have an item in
// `section` ("" for any), as indexes in `titles`, sorted by `sort` (by name
// after that).
std::vector<size_t> select_titles(const std::vector<Title>& titles, const std::string& query, Sort sort,
                                  const std::string& section = "");

// A name made for comparing: lower case, the Latin accents dropped
// ("Pokémon" -> "pokemon"); other scripts as they are.
std::string fold(const std::string& text);

// "Game [0100ABCD00010000][v0].nsp" -> "Game": a file name without its
// tags in brackets and its extension.
std::string clean_name(const std::string& name);

// The newest update the server has for `t` (-1: none).
long latest_update(const Title& t);

}   // namespace library
