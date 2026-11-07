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
// why not.
void load(const library::Server& server, const std::function<void(std::shared_ptr<const Loaded>, std::string)>& done);

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

// Removes an installed one's files and forgets it; "" or why not.
std::string remove(const std::string& id);

}   // namespace homebrew_flow
