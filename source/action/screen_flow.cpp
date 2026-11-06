// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/screen_flow.hpp"

#include <atomic>

#include <aurore.hpp>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace aurore::literals;

namespace
{
std::atomic<bool> s_active{ false };   // downloads running

void apply()
{
    aurore::ScreenRestOptions options;
    options.idleSeconds = config::get().screen_off_minutes * 60;
    options.warnSeconds = 10;
    // Never dark while the app is in the background (HOME pressed).
    options.allowed = []() { return s_active.load() && app::in_focus(); };
    options.onWarning = []() { ui::notify("playshelf/downloads/screen_off_warning"_i18n); };
    aurore::Application::setScreenRest(std::move(options));
}
}   // namespace

namespace screen_flow
{
void active(bool on)
{
    // Setting the options turns the panel back on and restarts the idle clock: only when this changes.
    if (s_active.exchange(on) != on) apply();
}

void changed()
{
    apply();
}

void stop()
{
    s_active = false;
    aurore::Application::setScreenRest({});   // the panel back on
}
}   // namespace screen_flow
