// titles — a catalogue as the Catalogue tab shows it: its items gathered by
// game (a base game, its updates newest first, its add-ons), then the games
// a search and a section keep, in the order asked. An item whose title id
// is unknown is a game of its own.
//
// Plain C++ for the host tests (tests/library_titles).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
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
    std::vector<std::string> sections;    // the ids of the sections its items are in
};

// BestKnown, BestReviewed and BestRated: by one of Atlas' scores (`Selection::score`), the best first.
enum class Sort : uint8_t { Name, Newest, Largest, Publisher, BestKnown, BestReviewed, BestRated };

std::vector<Title> group_titles(const Catalog& catalog);

// A name made for comparing: lower case, the Latin accents dropped
// ("Créatures" -> "creatures"); other scripts as they are.
std::string fold(const std::string& text);

// Sorts `items` by fold(name(item)), keeping the order of equal names. Each
// name is folded once: fold() allocates, and a comparison would fold both
// sides n·log n times.
template <typename T, typename Name>
void sort_folded(std::vector<T>& items, Name name)
{
    std::vector<std::pair<std::string, size_t>> keys;
    keys.reserve(items.size());
    for (size_t i = 0; i < items.size(); i++) keys.emplace_back(fold(name(items[i])), i);
    std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<T> sorted;
    sorted.reserve(items.size());
    for (const auto& k : keys) sorted.push_back(std::move(items[k.second]));
    items = std::move(sorted);
}

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
    bool     rebuilt = false;        // installed from a package rebuilt outside Nintendo's tools (see set_rebuilt): its update is inside it
};

// The base games installed from a package stored compressed and not signed
// by Nintendo (hacBrewPackLZ and the like): their update is built into the
// base game, and the official one patches the original, so installing it
// over would break the game. state_of() never offers an update for them.
// Set by action/rebuilt on the UI thread; read there too.
void set_rebuilt(std::set<uint64_t> base_ids);

// The games of `rebuilt` the console no longer has: they leave the list, but
// nothing is dropped when `installed` is empty (what is installed may not
// have been read) nor a game of `keep` (installed since that was read).
// True when a game left.
bool prune_rebuilt(std::set<uint64_t>& rebuilt, const std::map<uint64_t, uint32_t>& installed,
                   const std::set<uint64_t>& keep);

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
    // Atlas' scores of a game, by title id, for the Best… sorts: 0..100, 0 when not known (nullptr: none known).
    std::function<int(uint64_t, Sort)>      score;
    // Whether a game has the series and the studio asked (Atlas' traits: "" asks for none of that kind);
    // nullptr: none known, so a game is left out when either is asked.
    std::function<bool(uint64_t, const std::string&, const std::string&)> traits;
};

// The games whose name (or title id: 8 hex digits at least) holds `query` (case and the
// Latin accents aside; "" keeps every one), that have an item in `section`
// ("" for any) and that `selection` keeps, as indexes in `titles`, sorted by
// `sort` (by name after that; Newest by the facts' date when the catalogue
// has none, Publisher those with none last, the Best… sorts those with no score last).
std::vector<size_t> select_titles(const std::vector<Title>& titles, const std::string& query, Sort sort,
                                  const std::string& section = "", const Selection& selection = {});

}   // namespace library
