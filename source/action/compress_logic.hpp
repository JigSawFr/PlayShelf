// compress_logic — what must hold on the console before a game is compressed
// in place. Plain C++ so the host tests can run it (tests/compress_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include "util/patches.hpp"

namespace compress_logic
{

// The compressed program is not signed by Nintendo, so the loader (ldr: the
// signature of main.npdm's ACID) and the file system (fs: the signature of the
// NCA header) must both be patched, by sys-patch or by sigpatch files. Empty
// when they are; otherwise the reason, naming the module that is not.
std::string signature_blocker(const patches::Report& report);

}   // namespace compress_logic
