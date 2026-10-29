// insights — what PlayShelf makes of the console and its servers, whatever
// the tab: each server's catalogue read once a run (one after the other,
// once the console's games are), then, every time something changes (a
// catalogue, the console's games, the downloads, Nlib API's facts, the
// popular games, the wishlist, the title databases), the rows Home shows
// and what the notification bell says (action/notices): the updates and
// add-ons to get, the games new on the servers (library/seen), the games of
// the wishlist a server has, the updates the servers lack (the title
// databases know a newer one), the SD card nearly full, the downloads ended
// or failed. The console's games with something to get are also counted on
// Home's entry of the sidebar. UI thread only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "action/installed_flow.hpp"
#include "action/library_flow.hpp"
#include "library/recommend.hpp"

namespace insights
{

// A game of a catalogue.
struct Pick
{
    std::shared_ptr<const library_flow::Loaded> data;
    size_t                                      index = 0;

    const library::Title& title() const { return this->data->titles[this->index]; }
};

// A game of the console, and a catalogue that has it (none: data nullptr).
struct Played
{
    installed_flow::Game game;
    std::string          name;
    Pick                 found;
};

// A game the title databases date in the next weeks.
struct Coming
{
    uint64_t    id = 0;
    std::string name, publisher;
    uint32_t    date = 0;   // YYYYMMDD
};

// A game of the console whose newest update (the title databases') no
// server has yet.
struct Missing
{
    uint64_t    id = 0;
    std::string name;
    uint32_t    newest = 0;   // the update's version the databases know
    uint32_t    served = 0;   // the newest a server has, 0: none
};

// A game of the wishlist, and a catalogue that has it (none: data nullptr).
struct Wished
{
    uint64_t    id = 0;
    std::string name;
    Pick        found;
    bool        installed = false;
};

// A game suggested, and why (library/recommend).
struct Suggested
{
    Pick            pick;
    library::Reason reason;
};

// A game of a series the console has games of.
struct InSeries
{
    Pick        pick;
    std::string series;
};

struct Snapshot
{
    size_t console_games = 0, server_games = 0;
    size_t updates = 0, dlcs = 0;   // the console's games with a newer update; the add-ons missing
    size_t to_get_games = 0;        // the console's games with something to get: Home's badge
    size_t downloads = 0;           // waiting or running
    size_t never_played = 0;
    bool   loading = false;         // a catalogue is being read

    std::vector<Pick>    to_get;     // the console's games with something to get, the most played first
    std::vector<Played>  recent;     // played lately, the latest first
    std::vector<Pick>    added;      // new on a server (library/seen), the latest first
    std::vector<Pick>    newest;     // the servers' newest games the console has not
    std::vector<Pick>    month;      // released in the last month, the console has not
    std::vector<Pick>    popular;    // the servers' best known games the console has not
    std::vector<Pick>    trending;   // more read lately
    std::vector<Suggested> like;     // for the console's player, from every signal (library/recommend)
    std::vector<InSeries>  series;   // in the series of the console's games, the most played's first
    std::vector<Coming>  coming;     // coming out soon (the title databases)
    std::vector<Missing> missing;    // updates no server has yet
    std::vector<Wished>  wished;     // the wishlist, those a server has first

    installed_flow::Space space;
    // The console's games not played for months, the largest first.
    std::vector<Played> idle;
};

// Starts reading (the main screen is up). Once.
void start();

// Made again (Home shown: the console's games were read again), and the
// servers not read yet read.
void refresh();

// What it last made (empty before the first time).
std::shared_ptr<const Snapshot> now();

// Every update to get: asked first (how many, how large), then added to the
// downloads, the most played game's first, each from its best server.
void update_all();

// Told after every change, on the UI thread.
int  listen(std::function<void()> changed);
void unlisten(int id);

// A game's name: the console's (`console`), the title databases', Nlib
// API's, else its title id.
std::string name_of(uint64_t id, const std::string& console = "");
// A game's catalogue, the first server's read in this run that has it.
Pick found_of(uint64_t id);

// The day (YYYYMMDD) a game was first seen on its server, 0 when it was
// there from the first reading.
uint32_t first_seen(const Pick& pick);

// How much room is short: the SD card under a tenth free, or under 4 GB.
bool nearly_full(const installed_flow::Space& space);

}   // namespace insights
