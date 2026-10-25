// live_prefs — Preferences › Theme and Language applied at once, without
// restarting: the theme's variant set, the language's strings loaded again
// (Aurore, patch 0012), then the main screen built again on the tab that
// was shown, the focus on the line it was on (every view takes its colours,
// words and font when it is made; Chinese has its own font, ui::use_font_for
// and Aurore, patch 0013).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace live_prefs
{
// Right after borealis starts, which reads the console's own theme and
// language (what "system" means): those kept, and the strings loaded in the
// preferences' language.
void start();

// The variant of the theme in the preferences (config::Config::theme).
void apply_theme();

// The language the strings are in now: the preferences', or the console's.
std::string locale();

// After the preferences' theme or language changed: applied, and the screens
// built again.
void theme_changed();
void language_changed();
}   // namespace live_prefs
