// log_files — how logs/ is laid out, and how much of it is kept:
//
//   logs/reports/report_20261011_142530.txt   saved diagnostic reports
//   logs/crashes/crash_20261011_142530.txt     one file per crash (util/crash_report.hpp)
//   logs/sessions/session_20261011_142530.txt  everything logged during one run
//                                              (util/session_log.hpp)
//   logs/uploads/upload_20261011_142530.url    one per report sent online: an
//                                              Internet shortcut to its gist
//                                              (shortcut())
//   logs/<other folders>                       the app's own (PlayGuard's
//                                              play_timer/), left alone here
//
// The reports, the crashes, the sessions and the uploads are pruned with the limits of
// About › Help & diagnostics (config: logs_keep, logs_keep_days, logs_keep_mb), each folder on
// its own.
// The date in the name (local time, as the console shows it) is what counts,
// not the file's date, which the SD card does not always keep.
// Plain C++ (no Aurore): host-tested (tests/log_files).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace log_files
{

constexpr const char* REPORT = "report";
constexpr const char* CRASH  = "crash";
constexpr const char* UPLOAD = "upload";
constexpr const char* SESSION = "session";

// The extension of `prefix`'s files: ".url" for the uploads, ".txt" for the others.
const char* extension(const char* prefix);

// "report_20261011_142530.txt", "report_20261011_142530_2.txt" for n = 2
// (another one in the same second), "upload_20261011_142530.url". `stamp` is
// "YYYYMMDD_HHMMSS".
std::string name(const char* prefix, const std::string& stamp, unsigned n = 0);

// An upload's file: an Internet shortcut to `url`, which a computer opens in
// its browser (Windows, macOS, most Linux file managers), with when it was
// sent ("2026-10-11 14:25") and a removal link, when there is one (a bpa.st
// paste sent by an earlier version), in a section of its own. CRLF line ends, as Windows writes them.
std::string shortcut(const std::string& url, const std::string& removal, const std::string& sent);

// `t` as a name's stamp, in local time ("19700101_000000" when it cannot be
// converted).
std::string stamp(std::time_t t);

// Whether `name` is `prefix`_YYYYMMDD_HHMMSS[_n].txt; *stamp gets its date.
bool parse(const std::string& name, const char* prefix, std::string* stamp = nullptr);

// The names of `prefix` files in `dir`, newest first (the stamp sorts them;
// in a second, _n after the one without).
std::vector<std::string> list(const std::string& dir, const char* prefix);

// What is kept; 0: no limit for that one. A file goes when any limit says so.
struct Limits
{
    int count = 0;   // the newest `count`
    int days  = 0;   // dated less than `days` days before now
    int mb    = 0;   // the newest ones that fit in `mb` MB together
};

struct File
{
    std::string name;
    std::string stamp;
    uint64_t    bytes = 0;
};

// Which of `files` (newest first) go under `limits`, `cutoff` being the stamp
// `days` before now (stamp(now - days * 86400)). The newest file never goes:
// the report just saved, the crash that just happened.
std::vector<std::string> to_prune(const std::vector<File>& files, const Limits& limits, const std::string& cutoff);

// Writes `content` as a new `prefix` file of `dir` dated `now` (the first
// free name: _1, _2… within a second). Returns its path, or "" and *error.
std::string save(const std::string& dir, const char* prefix, const std::string& content, std::time_t now,
                 std::string* error = nullptr);

// Removes the `prefix` files of `dir` that `limits` leave out at time `now`.
// Returns how many were removed.
size_t prune(const std::string& dir, const char* prefix, const Limits& limits, std::time_t now);

}   // namespace log_files
