// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/frame_stats_flow.hpp"

#include <atomic>
#include <format>
#include <aurore.hpp>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#ifdef __SWITCH__
#include <switch.h>
#else
#include <ctime>
#endif

#include <aurore/cadran/ui/cadran_list.hpp>
#include <aurore/cadran/ui/effects.hpp>

#include "app.hpp"
#include "core/local_time.h"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/frame_stats.hpp"
#include "util/paths.hpp"

namespace frame_stats_flow
{

namespace
{
// A frame this long after the one before is not a frame but a pause (the
// app in the background, a dialog of the system): the second starts over.
constexpr uint64_t PAUSE_US = 2000000;

bool                s_running = false;
std::atomic<bool>   s_warned{ false };   // one log line when the SD card refuses, not one a second
uint64_t            s_last_wall_us = 0, s_last_cpu_us = 0, s_second_start_us = 0;
frame_stats::Second s_second;
std::string         s_screen = "?";
std::string         s_last;

uint64_t wall_us()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

// This thread's own running time, in microseconds.
uint64_t cpu_us()
{
#ifdef __SWITCH__
    // The kernel's count of the ticks spent on this thread, on every core:
    // InfoType_ThreadTickCount (25) on 13.0.0+, its deprecated id before.
    static u32 s_id = 25;
    u64 ticks = 0;
    if (R_FAILED(svcGetInfo(&ticks, s_id, CUR_THREAD_HANDLE, (u64)-1))) {
        if (s_id != 0xF0000002) {
            s_id = 0xF0000002;
            if (R_SUCCEEDED(svcGetInfo(&ticks, s_id, CUR_THREAD_HANDLE, (u64)-1))) return armTicksToNs(ticks) / 1000;
        }
        return 0;
    }
    return armTicksToNs(ticks) / 1000;
#elif defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts {};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#else
    return 0;
#endif
}

std::string mode()
{
#ifdef __SWITCH__
    std::string m = appletGetOperationMode() == AppletOperationMode_Handheld ? "handheld" : "docked";
#else
    std::string m = "desktop";
#endif
    // The glow turned off (Developer): its cost is the difference; the
    // lighter effects off the dock (ui/effects) leave it out too, and
    // Reduced motion keeps it still.
    if (!config::get().look.glow) m += "-noglow";
    else if (effects::light()) m += "-light";
    else if (effects::reduced()) m += "-reduced";
    return m;
}

std::string stamp()
{
    u64 posix = 0;
    LocalTime l {};
    if (!time_local_now(&posix, &l)) return "?";
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", (int)l.year, (int)l.month, (int)l.day,
                       (int)l.hour, (int)l.minute, (int)l.second);
}

void begin_second(uint64_t now)
{
    s_second          = frame_stats::Second();
    s_second_start_us = now;
}

// Appends `text` to the file (created 0644, as util/pt_log.cpp), `fresh`
// starting it over. Off the UI thread (ui::write_later): a slow SD card must
// not hold the frames being measured.
void write(const std::string& text, bool fresh)
{
    ui::write_later("frame_stats", [text, fresh]() {
        paths::ensure_dir(paths::logs_dir());
        const int fd = ::open(frame_stats::path().c_str(), O_WRONLY | O_CREAT | (fresh ? O_TRUNC : O_APPEND), 0644);
        FILE* f = fd < 0 ? nullptr : ::fdopen(fd, fresh ? "w" : "a");
        if (!f) {
            if (fd >= 0) ::close(fd);
            if (!s_warned.exchange(true)) aurore::Logger::warning("frame_stats: cannot write {}", frame_stats::path());
            return;
        }
        std::fputs(text.c_str(), f);
        std::fclose(f);
        s_warned = false;
    });
}

void flush()
{
    const std::string size = std::format("{}x{}", aurore::Application::windowWidth, aurore::Application::windowHeight);
    const std::string l    = frame_stats::line(stamp(), mode(), size, s_screen, s_second);
    s_last = l.substr(0, l.size() - 1);
    write(l, false);
}

void start()
{
    if (s_running) return;
    // Begun afresh: the file holds this run alone.
    write(frame_stats::header() + std::format("# PlayShelf {}\n", app::version()), true);
    s_running      = true;
    s_last_wall_us = 0;
    s_last.clear();
}
}   // namespace

void apply()
{
    if (app::dev_mode() && config::get().frame_stats) start();
    else stop();
}

void stop() { s_running = false; }

bool running() { return s_running; }

void sample()
{
    if (!s_running) return;
    const uint64_t now = wall_us(), cpu = cpu_us();
    if (s_last_wall_us == 0 || now - s_last_wall_us > PAUSE_US) {
        // The first frame, or the one after a pause: nothing to measure yet.
        s_last_wall_us = now;
        s_last_cpu_us  = cpu;
        begin_second(now);
        return;
    }
    // nanovg's own counters for the frame (Aurore's nvgDebugGetStats).
    int draws = 0, fill = 0, stroke = 0, text = 0;
    nvgDebugGetStats(aurore::Application::getNVGContext(), &draws, &fill, &stroke, &text);
    s_second.add(now - s_last_wall_us, cpu - s_last_cpu_us, draws, fill + stroke + text);
    s_last_wall_us = now;
    s_last_cpu_us  = cpu;
    if (now - s_second_start_us >= 1000000) {
        flush();
        begin_second(now);
    }
}

void set_screen(const std::string& name)
{
    if (name == s_screen) return;
    s_screen = name;
    if (s_running) s_last_wall_us = 0;   // the next frame starts the first second of this screen
}

std::string last() { return s_last; }

std::string screen() { return s_screen; }

}   // namespace frame_stats_flow
