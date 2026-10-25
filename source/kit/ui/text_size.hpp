// text_size — Preferences › Text size: "auto"
// (the default: larger while the console is docked, on a TV), "normal" or
// "large". Larger draws every label LARGE times its size
// (brls::Label::setFontScale, Aurore, patch 0007); the rail's too, the
// header's and the footer's a little less, each as its room allows
// (ui/cadran_frame). Applied at start-up,
// then again, live, when the setting changes or the console leaves or
// joins its dock: every label of every screen is laid out again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace text_size
{
constexpr float LARGE = 1.25f;

// The console on its dock (the desktop: PLAYSHELF_SIM_DOCKED=1).
bool docked();
// What the setting and the console's mode ask for now.
bool larger();
// brls::Label's scale from larger(); the screens laid out again when it changed.
void apply();
// apply() before the first view, then follows the dock. Once, at start-up.
void install();
}   // namespace text_size
