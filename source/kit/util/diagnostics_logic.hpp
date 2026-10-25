// diagnostics_logic — the parts of the diagnostic report (util/diagnostics)
// that read nothing themselves: sizes, the storage's name, the game patches'
// line. Plain C++ for the host tests (tests/diagnostics_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

#include "util/patches.hpp"
#include "core/sysinfo.h"

namespace diagnostic
{

// The storage the console started from, as the reports name it: "emuMMC",
// "sysMMC", nullptr when unknown.
const char* storage_name(SysStorage storage);

// "1.5 GiB", "312.4 MiB", "18 KiB" (rounded up).
std::string bytes(uint64_t n);

// The text up to its first line end (all of it without one).
std::string first_line(const std::string& text);

// The game patches' line of the report: sys-patch's state and why, its log,
// each module, what it left unpatched and the sigpatch files found (no
// personal data).
std::string patch_line(const patches::Report& pr);

}   // namespace diagnostic
