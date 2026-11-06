// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "util/screen_timer.hpp"

using namespace screen_timer;

constexpr int64_t MIN = 60 * 1000;

int main()
{
    // Off in the settings (0), or no download running: never darkens.
    {
        Timer t;
        CHECK(t.tick(100 * MIN, 0, false, 0, true) == Action::None);
        CHECK(t.tick(100 * MIN, 0, false, 5, false) == Action::None && t.phase() == Phase::Awake);
    }
    // Idle for the minutes: the warning, then the panel off after WARN_MS.
    {
        Timer t;
        CHECK(t.tick(4 * MIN, 0, false, 5, true) == Action::None);
        CHECK(t.tick(5 * MIN, 0, false, 5, true) == Action::ShowWarning && t.phase() == Phase::Warning);
        CHECK(t.tick(5 * MIN + WARN_MS - 1, 0, false, 5, true) == Action::None);
        CHECK(t.tick(5 * MIN + WARN_MS, 0, false, 5, true) == Action::PanelOff && t.phase() == Phase::Off);
        CHECK(t.tick(9 * MIN, 0, false, 5, true) == Action::None);
        // A pad input turns it on again, once; the idle clock is the screen's own input after that.
        CHECK(t.tick(9 * MIN + 1, 0, true, 5, true) == Action::PanelOn && t.phase() == Phase::Awake);
        CHECK(t.tick(9 * MIN + 2, 9 * MIN + 1, false, 5, true) == Action::None);
    }
    // An input during the warning cancels it.
    {
        Timer t;
        CHECK(t.tick(5 * MIN, 0, false, 5, true) == Action::ShowWarning);
        CHECK(t.tick(5 * MIN + 2000, 5 * MIN + 1000, false, 5, true) == Action::HideWarning && t.phase() == Phase::Awake);
        CHECK(t.tick(5 * MIN + 3000, 5 * MIN + 1000, false, 5, true) == Action::None);
    }
    // The downloads ending, or the setting going to 0, brings the panel back at once.
    {
        Timer t;
        t.tick(5 * MIN, 0, false, 5, true);
        CHECK(t.tick(5 * MIN + 1, 0, false, 5, false) == Action::HideWarning);
        t.tick(10 * MIN, 0, false, 5, true);
        t.tick(10 * MIN + WARN_MS, 0, false, 5, true);
        CHECK(t.phase() == Phase::Off);
        CHECK(t.tick(11 * MIN, 0, false, 5, false) == Action::PanelOn && t.phase() == Phase::Awake);
        t.tick(20 * MIN, 0, false, 5, true);
        t.tick(20 * MIN + WARN_MS, 0, false, 5, true);
        CHECK(t.tick(21 * MIN, 0, false, 0, true) == Action::PanelOn);
    }
    // After a wake-up the idle clock starts again, though the screen saw no input (it was blocked).
    {
        Timer t;
        t.tick(5 * MIN, 0, false, 5, true);
        t.tick(5 * MIN + WARN_MS, 0, false, 5, true);
        CHECK(t.tick(20 * MIN, 0, true, 5, true) == Action::PanelOn);
        CHECK(t.tick(20 * MIN + 1000, 0, false, 5, true) == Action::None && t.phase() == Phase::Awake);
        CHECK(t.tick(25 * MIN - 1, 0, false, 5, true) == Action::None);
        CHECK(t.tick(25 * MIN, 0, false, 5, true) == Action::ShowWarning);
    }
    // A button held at the moment the minutes are up keeps the screen on.
    {
        Timer t;
        CHECK(t.tick(5 * MIN, 0, true, 5, true) == Action::None && t.phase() == Phase::Awake);
    }
    CHECK_DONE("screen_timer");
}
