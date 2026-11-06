// The desktop build's core/boost: no CPU to raise, said in the log.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <aurore.hpp>

#include "core/boost.h"

void core_cpu_boost(bool on)
{
    aurore::Logger::info("sim: the CPU is {}", on ? "boosted" : "back to normal");
}
