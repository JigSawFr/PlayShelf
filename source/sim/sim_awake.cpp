// The desktop build's core/awake: no sleep to hold off, said in the log.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <aurore.hpp>

#include "core/awake.h"

void core_keep_awake(bool on)
{
    static bool s_on = false;
    if (on == s_on) return;
    s_on = on;
    aurore::Logger::info("sim: the console {} go to sleep by itself", on ? "does not" : "may again");
}
