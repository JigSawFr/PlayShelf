// screen_hooks — fills in ui::hooks (ui/hooks.hpp): what ui/ and view/ ask
// of the main screen, the tabs and a few screens and flows, which they do
// not include. Once, at start-up, before any screen opens.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

void install_screen_hooks();
