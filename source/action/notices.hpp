// notices — the notification bell of the main screen: what is worth the
// user's eye, gathered from the parts of PlayShelf that see it
// (notice_logic has the rules). Y on the main screen, or a tap on the bell,
// opens the list (activity/notices_activity); the bell in the header counts
// what is to do. Each entry can carry an action: what it says, done, or the
// screen where it is. UI thread only.
//
// The sources: Home's reading of the console and its servers (the updates
// and add-ons to get, the games new on the servers, the wishlist's games
// found, the updates the servers lack, the SD card's room), the downloads
// that end, and the kit's own news (ui::app_hooks: a new version, the build
// installed or gone back from, a report sent).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "action/notice_logic.hpp"

namespace notices
{

using Level = notice_logic::Level;

struct Entry
{
    notice_logic::Item    item;
    std::string           action_label;   // "" when there is nothing to do but read it
    std::function<void()> action;
};

// Reads what was put away at an earlier start, and fills in the kit's hooks
// (ui::app_hooks::notice_set and notice_clear). Once, at start-up.
void start();

// Adds or replaces the entry `id`. `action` runs from the list, after it closed.
// `key`: what brings it back once put away, when not its words
// (notice_logic::Store::set).
void set(const std::string& id, Level level, const std::string& title, const std::string& detail,
         const std::string& action_label = "", std::function<void()> action = nullptr, const std::string& key = "");
void clear(const std::string& id);
// Puts away an Info entry, remembered on the SD card until its text changes.
void dismiss(const std::string& id);
// Every entry for information put away.
void dismiss_all();

std::vector<Entry> list();
int action_count();   // the bell's number, with others_count()
// The other Boréale homebrews' notices to do or failed (none when
// Preferences › The other Boréale homebrews' notifications is off), as last
// read: at start-up (publish), every minute while PlayShelf is in focus, and
// when that preference changes (count_others).
int others_count();
void count_others();
int info_count();     // the entries for information: the bell's dot

// Called (UI thread) after every change of what the bell shows.
// MainActivity, once per start: PlayShelf in the Boréale homebrews' registry
// (sd:/config/boreale/apps/playshelf.conf) and its bell as they read it
// (sd:/config/boreale/notices/playshelf.json, action/shared_bell_logic),
// each written only when it changed. The bell is written again at each of
// its changes.
void publish();

int  listen(std::function<void()> fn);
void unlisten(int id);

}   // namespace notices
