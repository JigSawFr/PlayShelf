// frame_stats — the per-second frame statistics of the developer tools
// (About › Help & diagnostics › Developer › Frame statistics): how long the app's own thread
// works for each frame, how far apart the frames come and how many come late,
// so one screen can be weighed against another on the console, docked and
// handheld (the visual effects full against lighter, Preferences › Visual
// effects), in logs/frame_stats.txt. This part is plain C++ (no libnx, no UI) for the host
// tests (tests/frame_stats); action/frame_stats_flow runs it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace frame_stats
{

std::string path();   // logs_dir()/frame_stats.txt

// A frame that came this long after the one before is late: more than a
// 60 Hz frame and the next, the console having skipped a vsync.
constexpr uint64_t LATE_US = 20000;

// One second of frames. `cpu_us` is the thread's own running time over the
// frame, `wall_us` the time since the frame before; `draws` and `tris`
// (nanovg's draw calls and triangles, -1 when the build cannot count them)
// are those of the last frame, the same every frame on a still screen.
struct Second
{
    int      frames = 0;
    uint64_t wall_sum_us = 0, wall_max_us = 0;
    uint64_t cpu_sum_us = 0, cpu_max_us = 0;
    int      late  = 0;
    int      draws = -1, tris = -1;

    void add(uint64_t wall_us, uint64_t cpu_us, int draws, int tris);
};

// The first line of the file: what the columns are.
std::string header();

// One line (ending in "\n"): "<stamp>  <mode> <size>  <screen>  <fps> fps
// cpu <mean> ms (max <max>)  frame <mean> ms (max <max>)  late <n>  draws <n>
// tris <n>", the last two "-" when unknown; "no frame" for a second without
// any (the app in the background).
std::string line(const std::string& stamp, const std::string& mode, const std::string& size,
                 const std::string& screen, const Second& s);

}   // namespace frame_stats
