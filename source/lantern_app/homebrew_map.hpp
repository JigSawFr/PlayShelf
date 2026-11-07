// A Lantern server's homebrew catalogue (SPEC.md § 8) for the Homebrew tab:
// the entries paged from `GET /homebrew`, each kept whole (the install needs
// the package and its manifest) with the texts a screen shows made safe and
// the address of its file made absolute (https only). An entry with a bad
// id or a file that is not https:// is counted in `skipped` and left out.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <span>
#include <string>
#include <vector>

#include <lantern/lantern.hpp>

namespace lantern_app
{

struct HomebrewItem
{
    lantern::Homebrew entry;
    std::string       name, author, summary, description, version;
    std::string       url;    // the chosen package's file, absolute
    std::string       icon;   // https:// or ""
    uint64_t          size = 0;
};

struct HomebrewList
{
    std::vector<HomebrewItem> items;
    size_t                    skipped = 0;
    bool                      truncated = false;
};

constexpr size_t MAX_HOMEBREW = 2000;

// Adds `entries` to `list`; `origin` makes a relative file absolute and
// `arch` ("" when the platform has none) picks the package.
void add_homebrew(HomebrewList& list, std::span<const lantern::Homebrew> entries, const std::string& origin,
                  const std::string& arch);

// What this console holds, as the request of GET /homebrew/updates.
struct Held
{
    std::string id;
    uint64_t    build = 0;
};
lantern::HomebrewUpdatesRequest updates_request(std::span<const Held> held);

// Every page of the catalogue of `client` for `platform`, up to the bounds;
// an empty string when it was read, else why not.
std::string fetch_homebrew(lantern::Client& client, const std::string& platform, const std::string& origin,
                           const std::string& arch, HomebrewList& out);

// The entries of the server newer than `held`, same bounds.
std::string fetch_homebrew_updates(lantern::Client& client, std::span<const Held> held, const std::string& platform,
                                   const std::string& origin, const std::string& arch, HomebrewList& out);

}   // namespace lantern_app
