// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/screen_timer.hpp"

namespace screen_timer
{
Action Timer::tick(int64_t now_ms, int64_t last_input_ms, bool input_now, int minutes, bool active)
{
    const bool enabled = active && minutes > 0;
    // The idle clock starts at the last input, or at the wake-up: the input that woke the
    // panel was not the screen's (it was blocked), so last_input_ms does not show it.
    const int64_t idle_since = last_input_ms > woke ? last_input_ms : woke;
    const bool idle_long = enabled && now_ms - idle_since >= (int64_t)minutes * 60 * 1000;
    switch (state) {
    case Phase::Awake:
        if (idle_long && !input_now) {
            state = Phase::Warning;
            since = now_ms;
            return Action::ShowWarning;
        }
        return Action::None;
    case Phase::Warning:
        if (!enabled || input_now || last_input_ms > since) {   // the downloads ended, or someone is there
            state = Phase::Awake;
            return Action::HideWarning;
        }
        if (now_ms - since >= WARN_MS) {
            state = Phase::Off;
            return Action::PanelOff;
        }
        return Action::None;
    case Phase::Off:
        // The screen's own input is ignored while off: only the pad says (input_now).
        if (!enabled || input_now) {
            state = Phase::Awake;
            woke = now_ms;
            return Action::PanelOn;
        }
        return Action::None;
    }
    return Action::None;
}
}   // namespace screen_timer
