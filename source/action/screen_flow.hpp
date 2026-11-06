// screen_flow — the screen turned off during a long download (config
// screen_off_minutes, Downloads › Turn the screen off), to save the battery
// and the OLED panel: after that long without input a notice says so, ten
// seconds later the panel goes dark; any button or touch turns it back on and
// is not passed on to the screen (the inputs stay blocked until it is let go
// of). The downloads are never touched. The panel is back on when the downloads
// end, when the setting goes to 0, when the app loses focus and when it exits.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace screen_flow
{
// Downloads are running (true) or not: called where the console is kept awake.
void active(bool on);

// The setting changed: take it into account now.
void changed();

// The app exits: the panel back on, the inputs let go.
void stop();
}
