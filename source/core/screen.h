// core/screen — the panel turned off while downloads run (action/screen_flow):
// libnx's lbl backlight on the Switch, and the pad read directly, since the
// screen's own input is ignored while the panel is dark. The desktop build's
// version (sim/sim_screen.cpp) only logs.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// The backlight on or off (off fades for a moment). Safe to call twice.
void core_screen_set(bool on);

// Whether this build reads the pad itself (core_screen_input_active): the
// Switch does; the desktop does not, and the caller falls back on the app's input.
bool core_screen_polls_input(void);

// A button held or a finger on the screen, right now.
bool core_screen_input_active(void);

#ifdef __cplusplus
}
#endif
