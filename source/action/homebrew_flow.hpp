// homebrew_flow — what the Homebrew tab does with a server, in the
// background: its homebrew catalogue read (lantern_app/homebrew_map), kept
// for this run; a package fetched whole into the data folder and installed
// on the SD card (homebrew/install), the paths that need the user's consent
// asked first; what is installed kept in the book (homebrew.json).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "homebrew/install.hpp"
#include "lantern_app/homebrew_map.hpp"
#include "lantern_app/port_data.hpp"
#include "library/servers.hpp"

namespace homebrew_flow
{

struct Loaded
{
    library::Server           server;
    lantern_app::HomebrewList list;
};

// The catalogue last read in this run; nullptr when none.
std::shared_ptr<const Loaded> loaded();

// Reads `server`'s homebrew; `done` on the UI thread with it, or nullptr and
// why not. Asked while another read runs, it gets that read's answer (which
// may be another server's).
void load(const library::Server& server, const std::function<void(std::shared_ptr<const Loaded>, std::string)>& done);

// Reads the first paired server's homebrew in the background (the rail's
// badge counts its updates before the tab is opened); nothing when none.
void preload();

// How many of the catalogue last read have an update; 0 when none was read.
size_t updates_count();

// `fn` called on the UI thread when that count may have changed (the
// catalogue read, an install, a removal); an empty one forgets it.
void on_change(std::function<void()> fn);

// What is installed.
const homebrew::Book& book();

// "up to date", "update" or "new": how an entry stands against the book.
enum class Standing { New, Installed, Update };
Standing standing(const lantern_app::HomebrewItem& item);

// Installs entry `index` of `data`; `done` on the UI thread with "" or why
// not. One at a time.
void install(const std::shared_ptr<const Loaded>& data, size_t index, const std::function<void(std::string)>& done);
bool busy();
// What `done` gets when the user refused the paths that need consent.
inline constexpr const char* DECLINED = "declined";

// A port's game data looked for on the SD card, in the background (it hashes
// the files it finds); `done` on the UI thread.
void check_data(const lantern_app::HomebrewItem& item, const std::function<void(lantern_app::PortData)>& done);

// Removes an installed one's files and forgets it; "" or why not.
std::string remove(const std::string& id);

}   // namespace homebrew_flow
