// diagnostics — build and save the report users attach to bug reports.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>

#include "util/diagnostics_logic.hpp"

namespace diagnostic
{

// Header (app version, read-only / developer modes, firmware, Atmosphère,
// launch mode, storage, game patches, the app's own lines: add_header_line)
// + this run's start-up times + the
// app's own sections (add_section) + the logs (this run's session file, how
// the last one ended, memory, SD card, the newest crash files) + the last
// lines logged. Read-only; nothing that names the console's owner.
std::string current_report();

// A section of the app's (its own state, what it found on the console),
// given at start-up so the kit does not reach up to it: called for each
// report, in the order added, its text ending with a blank line.
void add_section(std::function<std::string()> section);

// A line of the app's at the end of the header ("compat      : 1"), given
// the same way: called for each report, in the order added, its text
// ending with a newline.
void add_header_line(std::function<std::string()> line);

// Saves as logs/reports/report_<timestamp>.txt (util/log_files.hpp). Returns the
// saved path, or an empty string (and *error set) on failure.
[[nodiscard]] std::string save(const std::string& report, std::string* error = nullptr);

}   // namespace diagnostic
