// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/frame_stats.hpp"

#include <format>

#include "app_id.h"
#include "util/paths.hpp"

namespace frame_stats
{

std::string path() { return paths::logs_dir() + "/frame_stats.txt"; }

void Second::add(uint64_t wall_us, uint64_t cpu_us, int d, int t)
{
    this->frames++;
    this->wall_sum_us += wall_us;
    if (wall_us > this->wall_max_us) this->wall_max_us = wall_us;
    this->cpu_sum_us += cpu_us;
    if (cpu_us > this->cpu_max_us) this->cpu_max_us = cpu_us;
    if (wall_us > LATE_US) this->late++;
    this->draws = d;
    this->tris  = t;
}

std::string header()
{
    return "# " APP_NAME " frame statistics: one line per second. time  mode size  screen  frames per second  "
           "cpu: the UI thread's own running time per frame, mean (max)  frame: time between two frames, "
           "mean (max)  late: frames more than 20 ms after the one before  draws, tris: nanovg's draw calls "
           "and triangles of the last frame (- when the build cannot count them)\n";
}

namespace
{
// Microseconds as "3.2" (ms, one decimal).
std::string ms(uint64_t us)
{
    return std::format("{}.{}", us / 1000, (us % 1000) / 100);
}

std::string count(int n) { return n < 0 ? "-" : std::to_string(n); }
}   // namespace

std::string line(const std::string& stamp, const std::string& mode, const std::string& size,
                 const std::string& screen, const Second& s)
{
    std::string out = stamp + "  " + mode + " " + size + "  " + screen + "  ";
    if (s.frames == 0) return out + "no frame\n";
    const uint64_t n = (uint64_t)s.frames;
    out += std::to_string(s.frames) + " fps  cpu " + ms(s.cpu_sum_us / n) + " ms (max " + ms(s.cpu_max_us) + ")  frame "
         + ms(s.wall_sum_us / n) + " ms (max " + ms(s.wall_max_us) + ")  late " + std::to_string(s.late) + "  draws "
         + count(s.draws) + "  tris " + count(s.tris) + "\n";
    return out;
}

}   // namespace frame_stats
