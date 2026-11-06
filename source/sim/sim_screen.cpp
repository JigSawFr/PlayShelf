// The desktop build's core/screen: no panel to turn off, said in the log.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <aurore.hpp>

#include "core/screen.h"

void core_screen_set(bool on)
{
    static bool s_on = true;
    if (on == s_on) return;
    s_on = on;
    aurore::Logger::info("sim: the screen goes {}", on ? "back on" : "dark");
}

bool core_screen_polls_input(void)
{
    return false;
}

bool core_screen_input_active(void)
{
    return false;
}
