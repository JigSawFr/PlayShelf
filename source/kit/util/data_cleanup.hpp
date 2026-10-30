// data_cleanup — what the developer tools' "Clear the cache" and "Delete the
// log files" remove from the app's data folder, and the removal itself.
// Folders stay (the icon store keeps writing to its own while the app runs);
// only files go. Never the settings, the change history, backups, profiles,
// exports or the GitHub sign-in. No Aurore: host-tested (tests/data_cleanup).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace data_cleanup
{

// Files to remove (full paths, sorted) and how much they hold.
struct Plan
{
    std::vector<std::string> files;
    uint64_t bytes = 0;
};

// Every file under `data_dir`/cache and each system's
// `data_dir`/nand/<key>/cache, in their subfolders too: what the app reads
// again from the console (or GitHub) when needed, such as the game icons and
// their language or the development build list (util/paths.hpp).
Plan cache(const std::string& data_dir);

// Every file under `data_dir`/logs and its subfolders (crashes/, reports/…)
// except what is not a log: uploads/ (the links of the reports sent online;
// the limits of About › Help & diagnostics prune it), the files the app
// named with keep_log(), nor `session`, this run's session file (sessions/<name>, util/session_log.hpp),
// still being written.
Plan logs(const std::string& data_dir, const std::string& session = "");

// Leaves `rel`, a path from `data_dir`/logs ("play_timer/block.json"), out of
// logs(): a file the app keeps there that is not a log. Called at start-up,
// before any logs().
void keep_log(std::string rel);

struct Outcome
{
    size_t   removed = 0;
    uint64_t bytes   = 0;   // of the files removed
    size_t   failed  = 0;   // there, and not removed
};

// Removes the plan's files. One already gone (removed since the plan was
// made) is neither removed nor failed.
Outcome remove(const Plan& plan);

// A size as a whole number of KB (under 1 MB; a non-empty file is at least
// 1 KB) or of MB (rounded, 1 MB at least): no decimals, whose separator
// would depend on the language.
enum class Unit
{
    KB,
    MB,
};
Unit size_unit(uint64_t bytes, uint64_t* value);

}   // namespace data_cleanup
