// crash_report — the app's own crash files, logs/crashes/crash_<date>.txt
// (util/log_files.hpp), one per crash:
//
//   2026-10-15 14:25:30  <APP_NAME> 1.2.0 (abc1234): cpu exception: data abort ...
//   system  : firmware 22.5.0, Atmosphère 1.11.1, sysMMC, over a game
//   session : logs/sessions/session_20261015_142010.txt, 320 s after the start
//
//   <what happened: the backtrace and registers, the exception's message...>
//
//   last lines logged (oldest first):
//   <session_log::recent()>
//
// Written by the CPU exception handler (core/crash_handler.c), by the
// handler of an uncaught C++ exception (main.cpp) and at the start after a
// run that never ended (main.cpp). The first line starts with its date.
//
// write() allocates nothing and takes no lock: it runs on a thread that
// crashed, maybe in the allocator. Plain C++: host-tested (tests/session_log).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <ctime>
#include <string>

#include "util/crash_dump.h"

namespace crash_report
{

// Where the files go and what to say of this run, kept in fixed buffers.
// `build`: "<APP_NAME> 1.2.0 (abc1234)"; `system`: the console ("firmware
// 22.5.0, Atmosphère 1.11.1, sysMMC, over a game"), "" until known;
// `session`: this run's session file name ("" without one). Called again
// when one of them becomes known; the time since the first call is the
// "after the start" of the files.
void prepare(const std::string& dir, const std::string& build, const std::string& system, const std::string& session);

// A new crash file dated `now`: the lines above, `details` (null for none)
// and, with `recent`, the last lines logged. Returns its name
// ("crash_20261015_142530.txt", in a buffer of its own until the next
// call), "" when it could not be written.
const char* write(std::time_t now, const char* summary, const char* details, bool recent = true) noexcept;

}   // namespace crash_report

// The CPU exception handler's: the dump as the details
// (crash_dump_format), then session_log::end_crash().
extern "C" void crash_report_cpu(const CrashDump* dump);
