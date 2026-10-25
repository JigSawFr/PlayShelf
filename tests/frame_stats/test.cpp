// Host tests for source/util/frame_stats.cpp: one second of frames summed
// (mean and maximum, the late ones counted, the last frame's draw counts),
// its line (milliseconds to one decimal, "-" for counts the build lacks) and
// the line of a second without a frame.
#include "check.h"
#include <string>

#include "util/frame_stats.hpp"

int main()
{
    frame_stats::Second s;
    CHECK(s.frames == 0 && s.draws == -1 && s.tris == -1);
    s.add(16667, 3200, 93, 2300);
    s.add(16666, 5100, 93, 2300);
    s.add(33333, 2800, 174, 6600);   // a skipped vsync
    CHECK(s.frames == 3);
    CHECK(s.wall_sum_us == 66666 && s.wall_max_us == 33333);
    CHECK(s.cpu_sum_us == 11100 && s.cpu_max_us == 5100);
    CHECK(s.late == 1);
    CHECK(s.draws == 174 && s.tris == 6600);

    const std::string l = frame_stats::line("2026-10-11 14:03:12", "docked", "1920x1080", "tab/cadran", s);
    CHECK(l == "2026-10-11 14:03:12  docked 1920x1080  tab/cadran  3 fps  cpu 3.7 ms (max 5.1)  "
               "frame 22.2 ms (max 33.3)  late 1  draws 174  tris 6600\n");

    // The build without nanovg's counters: "-", never a figure of its own.
    frame_stats::Second u;
    for (int i = 0; i < 60; i++) u.add(16667, 900, -1, -1);
    const std::string m = frame_stats::line("2026-10-11 14:03:13", "handheld", "1280x720", "tab/dashboard", u);
    CHECK(m == "2026-10-11 14:03:13  handheld 1280x720  tab/dashboard  60 fps  cpu 0.9 ms (max 0.9)  "
               "frame 16.6 ms (max 16.6)  late 0  draws -  tris -\n");

    // No frame in the second (the app was in the background).
    CHECK(frame_stats::line("2026-10-11 14:03:14", "desktop", "1280x720", "tab/dashboard", frame_stats::Second())
          == "2026-10-11 14:03:14  desktop 1280x720  tab/dashboard  no frame\n");

    CHECK(frame_stats::header().rfind("# PlayShelf frame statistics", 0) == 0);
    CHECK(frame_stats::path().size() > std::string("/frame_stats.txt").size());
    return CHECK_DONE("frame statistics assertions passed");
}
