// local_backups — a game's saves kept on the SD card as zip archives
// (saves/archive.hpp), beside the copy the restore of a server backup already
// keeps: data_dir()/saves/<TITLE ID>/<date>_<time>_<user>.zip, one file per
// user of the console. Only the last few of each user are kept; a file this
// does not name (the restore's own copy, one the user put there) is never
// counted, never deleted. Plain C++, host-tested (tests/local_backups).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace local_backups
{

constexpr size_t KEEP = 3;   // the copies kept of each user's save of a game

// data_dir()/saves/<TITLE ID>: where a game's copies are.
std::string dir_of(const std::string& data_dir, uint64_t title_id);

// 16 hex digits naming a user of the console, from its two uid words (the same user gives the same
// tag; two users giving one is harmless: they share their copies' count).
std::string user_tag(uint64_t uid0, uint64_t uid1);

// "2026-11-14_11-12-54_<tag>.zip" from "2026-11-14", "11:12:54" (the colons are not for a file name).
std::string file_name(const std::string& date, const std::string& time, const std::string& tag);

// Whether `name` is one of ours (the shape file_name makes), and its tag.
bool is_ours(const std::string& name, std::string* tag = nullptr);

// Of the file names of a game's folder, those to delete so that `keep` of each user's remain: the
// oldest, by the date and time in the name. Names that are not ours are never in it.
std::vector<std::string> expired(const std::vector<std::string>& names, size_t keep = KEEP);

// One of our copies, read from its name.
struct Copy
{
    std::string name;   // the file name
    std::string date;   // "2026-11-14"
    std::string time;   // "11:12:54"
    std::string tag;    // the user's (user_tag)
};

// Our copies among `names`, the newest first; what is not ours is left out.
std::vector<Copy> copies(const std::vector<std::string>& names);

// "before_restore_2026-11-14_11-12-54.zip": the save a restore replaces, kept first. Not ours by
// design: the count above never deletes it.
std::string safety_name(const std::string& date, const std::string& time);

}   // namespace local_backups
