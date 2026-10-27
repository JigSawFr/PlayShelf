// titles — a catalogue as the Catalogue tab shows it: its items gathered by
// game (a base game, its updates newest first, its add-ons), then the games
// a search and a section keep, in the order asked. An item whose title id
// is unknown is a game of its own.
//
// Plain C++ for the host tests (tests/library_titles).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "library/catalog.hpp"
#include "library/facts.hpp"

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

enum class Sort : uint8_t { Name, Newest, Largest, Publisher };

std::vector<Title> group_titles(const Catalog& catalog);

// A name made for comparing: lower case, the Latin accents dropped
// ("Pokémon" -> "pokemon"); other scripts as they are.
std::string fold(const std::string& text);

// "Game [0100ABCD00010000][v0].nsp" -> "Game": a file name without its
// tags in brackets and its extension.
std::string clean_name(const std::string& name);

// The newest update the server has for `t` (-1: none).
long latest_update(const Title& t);

// What the console has of a game, against what the server offers
// (`installed`: title id -> highest version installed, any kind).
struct TitleState
{
    bool     base = false;           // its base game is installed
    uint32_t version = 0;            // the version it runs: its update's, else the base game's
    bool     update_available = false;   // the server has a newer update than that
    size_t   dlcs_missing = 0;       // add-ons the server has and the console has not
};
TitleState state_of(const Title& t, const Catalog& catalog, const std::map<uint64_t, uint32_t>& installed);

// The item installed already, at this version or a newer one.
bool is_installed(const Item& item, const std::map<uint64_t, uint32_t>& installed);

// What a list keeps beyond its search and its section: a filter, with what
// is known of each game (its facts) and what the console has of it.
struct Selection
{
    Filter                                  filter;
    std::function<Facts(uint64_t)>          facts;   // by title id; nullptr: none known
    std::function<TitleState(const Title&)> state;   // nullptr: the filter's state is not applied
};

// The games whose name (or title id: 8 hex digits at least) holds `query` (case and the
// Latin accents aside; "" keeps every one), that have an item in `section`
// ("" for any) and that `selection` keeps, as indexes in `titles`, sorted by
// `sort` (by name after that; Newest by the facts' date when the catalogue
// has none, Publisher those with none last).
std::vector<size_t> select_titles(const std::vector<Title>& titles, const std::string& query, Sort sort,
                                  const std::string& section = "", const Selection& selection = {});

}   // namespace library
