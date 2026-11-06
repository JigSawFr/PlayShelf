// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/screen.h"

#include <switch.h>

static bool s_lbl_ready;
static bool s_pad_ready;
static bool s_touch_ready;
static PadState s_pad;
static bool s_off;

static bool lbl_ready(void)
{
    if (!s_lbl_ready) s_lbl_ready = R_SUCCEEDED(lblInitialize());
    return s_lbl_ready;
}

void core_screen_set(bool on)
{
    if (on == !s_off) return;
    if (!lbl_ready()) return;
    if (on) lblSwitchBacklightOn(0);
    else lblSwitchBacklightOff(500 * 1000 * 1000ull);   // fades over half a second
    s_off = !on;
}

bool core_screen_polls_input(void)
{
    return true;
}

bool core_screen_input_active(void)
{
    if (!s_pad_ready) {
        padConfigureInput(8, HidNpadStyleSet_NpadStandard);
        padInitializeAny(&s_pad);
        s_pad_ready = true;
    }
    padUpdate(&s_pad);
    if (padGetButtons(&s_pad) != 0) return true;
    if (!s_touch_ready) {
        hidInitializeTouchScreen();
        s_touch_ready = true;
    }
    HidTouchScreenState touch = { 0 };
    if (hidGetTouchScreenStates(&touch, 1) > 0 && touch.count > 0) return true;
    return false;
}
