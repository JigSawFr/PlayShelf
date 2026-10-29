// screen_hooks — fills in Aurore's ui::hooks (Aurore/cadran/ui/hooks.hpp)
// and the kit's ui::app_hooks (ui/app_hooks.hpp): what Cadran and the kit ask
// of the main screen, the tabs, PlayShelf itself and a few screens and flows,
// which they do not include. Once, at start-up, before any screen opens (and
// before aurore::Application::init(), which reads the look's preferences).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

void install_screen_hooks();
