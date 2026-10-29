// update_flow — "Check for updates", the screen that offers a newer release
// (activity/update_activity.hpp) and installing it the way About › Update
// with says: in place (PlayShelf downloads the release, checks it, replaces
// itself and restarts, as Developer › Install another build does),
// through sphaira or the Homebrew App Store, or by hand. While no way is
// chosen, the first update asks for one.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <functional>
#include <string>
#include <vector>

#include "util/dev_builds.hpp"
#include "util/update.hpp"

namespace update_flow
{

// What a check found: the verdict, and the latest release's file and notes
// when it is newer (has_release false when they could not be read).
struct Offer
{
    update::Result    result;
    bool              has_release = false;
    dev_builds::Build release;
};

// The values of About › Update with, in the order listed ("" is none chosen).
extern const std::vector<std::string> METHODS;

// Checks in the background, then shows the update screen when there is a
// newer version, else says what it found.
void check_now();

// This session's update check, for a screen that only reads it (the firmware
// screen, its toast): the request under way, or the last one that reached
// the server, else a new one. `done` runs on the UI thread with the result,
// recorded as check_now() records it (the notification bell, the day).
void check_once(std::function<void(const Offer&)> done);

// At start-up: checks when Check for updates every day is on (at most once a
// day) or when the last check found a newer version; the update screen comes
// up on `main` once nothing else is in front of it, unless postponed.
void at_start(aurore::Activity* main);

// Installs the newer version the way About › Update with says, asking for
// that way first when none is chosen.
void update_now();

// The list of About › Update with; `then` runs once one is picked and saved.
// `first`: the update screen's question (the recommended way marked).
void choose_method(std::function<void()> then, bool first = false);
// "In PlayShelf", "sphaira"…, "Not chosen yet".
std::string method_text();

// "Later": 1 day, 1 week or 30 days for `version`, then `done`.
void postpone(const std::string& version, std::function<void()> done);

// "PlayShelf 1.1.0 supports firmware 24.0.0 …" for the firmware screen.
std::string status_text(const update::Result& result, const std::string& firmware);

// "Update now" / "Update with sphaira" / "How to update" (label of the update actions).
std::string update_label();

}   // namespace update_flow
