// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/boost.h"

#include <switch.h>

void core_cpu_boost(bool on)
{
    appletSetCpuBoostMode(on ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal);
}
