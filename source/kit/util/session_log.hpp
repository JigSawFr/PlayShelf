// session_log — every line the app logs (Aurore's Logger, which only
// reaches a PC over nxlink) kept on the SD card, one file per run:
//
//   logs/sessions/session_20261015_142530.txt
//
// It starts with a header (the build, when it started), then one line per
// message ("14:25:31.042 I start-up: first screen ready after 2913 ms"), and
// ends with "=== end: clean exit ===" when the app quits normally, or
// "=== end: crash, see logs/crashes/crash_<date>.txt ===" when it caught its
// own crash. A file without an end line is a run that stopped without
// either (a crash outside the app's handlers, the console turned off, the
// software closed from the HOME menu while it hung): the next start says so
// in a crash file (main.cpp, unfinished()).
//
// The lines are written to the file together, not one by one (each write
// opens and closes it, under Aurore's log lock): they wait in memory, up to
// PENDING_BYTES, and go FLUSH_MS after the first of them (at the next line,
// or flush() from the app's main loop: due()), at once for a warning or an
// error, and before the end line, a crash's too (end_crash() writes them
// with no lock). Only a run that stops without either (the console turned
// off) loses the last lines that waited, a few info lines at most.
//
// The last RING_LINES lines also stay in memory, for the crash files and
// the diagnostic report: that part allocates nothing and takes no lock
// (recent()), as the CPU exception handler reads it.
// The files are pruned with the reports (the limits of About › Help & diagnostics).
// Plain C++ (no Aurore): host-tested (tests/session_log).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <ctime>
#include <string>

namespace session_log
{

// Past this size a session file stops growing (one line says so): a message
// repeated every frame must not fill the SD card. The lines still reach the
// memory kept for the crash files.
constexpr size_t MAX_BYTES = 1024 * 1024;

// The lines kept in memory, each cut to LINE_BYTES.
constexpr size_t RING_LINES = 80;
constexpr size_t LINE_BYTES = 240;

// The lines waiting to be written, at most, and for how long.
constexpr size_t PENDING_BYTES = 16 * 1024;
constexpr long long FLUSH_MS = 500;

// What unfinished() and tail_of() read of a file: its end.
constexpr size_t TAIL_BYTES = 16 * 1024;

// What a finished file's last line starts with.
constexpr const char* END = "=== end: ";

// One line as written: "14:25:31.042 W text\n", the time in local time,
// `level` one of E, W, I, D, V. A message of several lines keeps them, the
// next ones indented by four spaces.
std::string format(std::time_t t, int ms, char level, const std::string& text);

// Writes all of `data` to `fd`, retrying short writes; false on an error.
// No allocation (also the crash file's, util/crash_report).
bool write_all(int fd, const char* data, size_t size);

// Starts this run's file in `dir` (created when missing), dated `now`:
// `header` (lines ending in "\n"), then the lines logged before it from
// memory. Returns its name, or "" when the SD card refused (the lines are
// still kept in memory).
std::string start(const std::string& dir, std::time_t now, const std::string& header);

// Logs one line: in memory, and in the file when one is open (at once for
// `level` E or W, else with the next ones).
void add(std::time_t t, int ms, char level, const std::string& text);

// Whether lines have waited FLUSH_MS (no lock: for a main loop), and the
// waiting lines written now (before reading this run's file, for one).
bool due();
void flush();

// The lines still waiting, then "=== end: <reason> ===", once. Nothing
// after it is written to the file.
void end(const std::string& reason);

// The same from the crash handlers: the lines still waiting, then
// "=== end: crash, see logs/crashes/<name> ===", with no allocation and no
// lock.
void end_crash(const char* crash_name) noexcept;

// The lines kept in memory, oldest first, each ending in "\n", as many of
// the last ones as fit in `size` (always NUL-terminated). No allocation, no
// lock. Returns the length written.
size_t recent(char* out, size_t size) noexcept;
std::string recent();

// The name of this run's file ("" when none).
std::string current_name();

// How long ago start() was called, in seconds (0 before).
long long uptime_s();

// Whether a session file's text ends with its end line.
bool finished(const std::string& text);

// Why a session file ended: what its end line says ("clean exit", "crash,
// see logs/crashes/crash_20261015_142530.txt"), "" without one.
std::string end_reason(const std::string& text);

// The last `n` lines of `text`.
std::string last_lines(const std::string& text, size_t n);

// The last TAIL_BYTES of the file at `path`, from a whole line (false when
// it cannot be read): enough for its end line and its last lines.
bool tail_of(const std::string& path, std::string* out);

// The newest session file of `dir` other than `current`, when it has no end
// line: its name and the end of its text (tail_of()). False when there is
// none, or when it ended.
struct Unfinished
{
    std::string name;
    std::string text;
};
bool unfinished(const std::string& dir, const std::string& current, Unfinished* out);

// The memory and the file forgotten (no end line written): host tests only.
void reset();

}   // namespace session_log
