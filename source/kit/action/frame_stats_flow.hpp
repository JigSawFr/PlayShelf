// frame_stats_flow — runs the frame statistics (util/frame_stats) of Tools ›
// Developer: every frame of the main loop is timed, and one line per second
// goes to logs/frame_stats.txt (off the UI thread, with the other SD-card
// writes: the file must not hold the frames it measures), the file begun
// afresh each time recording starts. Its switch is remembered (config
// frame_stats) and only acts while the developer mode is on.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace frame_stats_flow
{

// Starts or stops recording, as config frame_stats and the developer mode
// say. At start-up, and whenever either changes.
void apply();
// Stops; the lines already asked are written by finish_background.
void stop();
bool running();

// Once per main-loop iteration, right after the frame (main.cpp).
void sample();

// The screen on top, as the lines name it ("tab/cadran"): set by each tab as
// it appears, and by the screens worth measuring on their own. A change
// starts a new second, so no line mixes two screens.
void set_screen(const std::string& name);
// The name set last ("" before the first).
std::string screen();

// The last line written, without its newline; "" until the first.
std::string last();

}   // namespace frame_stats_flow
