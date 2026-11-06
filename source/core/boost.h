// core/boost — the CPU raised for loading work (an install, a compression):
// libnx's appletSetCpuBoostMode on the Switch (FastLoad, then Normal after);
// nothing on the desktop (sim/sim_boost.cpp logs it). Callers go through
// util/cpu_boost, which counts who asked.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void core_cpu_boost(bool on);

#ifdef __cplusplus
}
#endif
