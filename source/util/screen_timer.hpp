// screen_timer — when the screen goes dark while downloads run (config
// screen_off_minutes): after that long without input a warning shows for
// WARN_MS, then the panel is turned off; an input turns it back on. Pure
// logic with the clock given, so it is tested on the host (action/screen_flow
// does the drawing and the panel).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>

namespace screen_timer
{
constexpr int64_t WARN_MS = 10 * 1000;   // the warning before the panel goes off
constexpr int64_t TAKEOVER_MS = 400;     // after waking, how long inputs stay ignored (the press that woke)

enum class Phase { Awake, Warning, Off };
enum class Action { None, ShowWarning, HideWarning, PanelOff, PanelOn };

class Timer
{
public:
    // `minutes` 0 never darkens. `active`: downloads running (the console is
    // kept awake). `now_ms` and `last_input_ms` come from the same clock.
    // `input_now`: an input is held or just came, read from the pad itself
    // (needed while the panel is off, when the screen's own input is ignored).
    Action tick(int64_t now_ms, int64_t last_input_ms, bool input_now, int minutes, bool active);

    Phase phase() const { return state; }

private:
    Phase   state = Phase::Awake;
    int64_t since = 0;   // when the warning showed
    int64_t woke = 0;    // when the panel last came back on
};
}   // namespace screen_timer
