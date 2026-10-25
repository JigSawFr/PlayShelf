// core/awake — the console kept from going to sleep by itself (the
// automatic sleep after a while without input) while downloads run: libnx's
// appletSetAutoSleepDisabled on the Switch, the setting it had put back
// after; nothing on the desktop (sim/sim_awake.cpp logs it).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void core_keep_awake(bool on);

#ifdef __cplusplus
}
#endif
