// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/screen_flow.hpp"

#include <memory>

#include <aurore.hpp>

#include "app.hpp"
#include "core/screen.h"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/screen_timer.hpp"

using namespace aurore::literals;

namespace
{
screen_timer::Timer                       s_timer;
std::unique_ptr<aurore::RepeatingTimer>   s_tick;
bool s_active = false;       // downloads running
bool s_blocking = false;     // the screen's inputs are blocked (dark, or waiting for the press to be let go)
bool s_off = false;          // the panel is dark
int64_t s_off_at = 0;        // when it went dark (the desktop's way to tell an input)

int64_t now_ms()
{
    return (int64_t)(aurore::getCPUTimeUsec() / 1000);
}

void block(bool on)
{
    if (on == s_blocking) return;
    s_blocking = on;
    if (on) aurore::Application::blockInputs(true);
    else aurore::Application::unblockInputs();
}

void panel(bool on)
{
    s_off = !on;
    if (!on) s_off_at = now_ms();
    core_screen_set(on);
}

// Everything back as it was.
void restore()
{
    panel(true);
    block(false);
}

void step()
{
    const bool polls = core_screen_polls_input();
    const bool held = polls ? core_screen_input_active() : false;
    const int64_t now = now_ms();
    int64_t last = (int64_t)(aurore::Application::getLastInputTime() / 1000);
    // The desktop cannot read a pad while the screen's input is blocked (it is not blocked there): its own input tells.
    const bool input = held || (!polls && s_off && last > s_off_at);

    // Waiting for the press that woke it to be let go: only then the screen takes input again.
    if (s_blocking && !s_off) {
        if (!held) block(false);
        return;
    }
    if (!app::in_focus() && (s_off || s_timer.phase() != screen_timer::Phase::Awake)) {
        // HOME pressed or the app sent to the background: never leave it dark.
        s_timer = screen_timer::Timer();
        restore();
        return;
    }
    switch (s_timer.tick(now, last, input, config::get().screen_off_minutes, s_active)) {
    case screen_timer::Action::ShowWarning:
        ui::notify("playshelf/downloads/screen_off_warning"_i18n);
        break;
    case screen_timer::Action::PanelOff:
        if (polls) block(true);   // the screen below must not get the press that wakes it
        panel(false);
        break;
    case screen_timer::Action::PanelOn:
        panel(true);   // inputs stay blocked until the press is let go (above)
        if (!polls) block(false);
        break;
    default:
        break;
    }
    if (!s_active && s_timer.phase() == screen_timer::Phase::Awake && !s_blocking && s_tick) s_tick->stop();
}

void ensure_running()
{
    if (!s_tick) {
        s_tick = std::make_unique<aurore::RepeatingTimer>();
        s_tick->setPeriod(250);
        s_tick->setCallback([]() { step(); });
    }
    if (!s_tick->isRunning()) s_tick->start();
}
}   // namespace

namespace screen_flow
{
void active(bool on)
{
    s_active = on;
    if (on && config::get().screen_off_minutes > 0) ensure_running();
}

void changed()
{
    if (config::get().screen_off_minutes > 0 && s_active) ensure_running();
}

void stop()
{
    if (s_tick) s_tick->stop();
    s_timer = screen_timer::Timer();
    restore();
}
}   // namespace screen_flow
