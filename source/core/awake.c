// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/awake.h"

#include <switch.h>

static bool s_on;
static bool s_saved_valid;
static bool s_saved;

void core_keep_awake(bool on)
{
    if (on == s_on) return;
    if (on) {
        s_saved_valid = R_SUCCEEDED(appletIsAutoSleepDisabled(&s_saved));
        appletSetAutoSleepDisabled(true);
    } else {
        appletSetAutoSleepDisabled(s_saved_valid ? s_saved : false);
    }
    s_on = on;
}
