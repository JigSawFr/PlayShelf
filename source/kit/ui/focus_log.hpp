// focus_log — for the desktop's smoke test (tools/desktop_smoke.py, go_to()):
// with PLAYSHELF_SIM_FOCUS_LOG=1, each line that takes the focus logs its
// title ("focus: Check for updates"), so the test reaches a line by its
// words, not by counting presses. Nothing on the console or in a browser.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace focus_log
{
// Once, at start-up (main.cpp); does nothing without the variable.
void install();
}   // namespace focus_log
