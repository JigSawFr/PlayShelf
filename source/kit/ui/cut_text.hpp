// cut_text — the desktop's check for texts cut short (tools/cut_text_check.py):
// with PLAYSHELF_SIM_CUT_TEXT=1, once a second, every label on the screen on
// top that its box cuts (drawn with "…", borealis patch 0008) goes to
// cut_text.txt in the working directory, once each: the screen's name (frame_stats_flow::screen,
// "… › dialog" or "… › picker" for one opened over it), a tab, then the label's whole text. Nothing on the console or in a browser.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace cut_text
{
// Once per main-loop iteration (main.cpp); does nothing without the variable.
void sample();
}   // namespace cut_text
