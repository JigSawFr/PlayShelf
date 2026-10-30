// startup — how long each step of the start-up took, for the diagnostic
// report: on the console the screen stays black until the graphics are up,
// and the log lines saying how long it took only reach a PC over nxlink.
//
// Each step is marked once, when it ends, on a monotonic clock (the system
// tick on the Switch). The first mark is the first code of the app that runs:
// Aurore's userAppInit, called by libnx before main (core/startup_hook.c;
// not on desktop, which starts at main). The time hbloader takes to load the
// .nro comes before any of it and is not measured.
// Plain C with no libnx (but the clock), so it can be marked before the C++
// static constructors run, and the host tests can feed it made-up times.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    StartupStep_ServicesBegin,   // userAppInit called: libnx's own services are up
    StartupStep_Services,        // it returned: sockets, romfs, settings, network (Aurore's switch_wrapper.c)
    StartupStep_Main,            // main() entered: the C++ static constructors ran
    StartupStep_Migrations,      // the data folder of older versions' layouts moved, once (util/migrations.hpp)
    StartupStep_Session,         // this run's session file, the last run checked for a crash
    StartupStep_Rollback,        // rollback_flow::at_start: a build that never reached its first screen
    StartupStep_System,          // the system (sysMMC or emuMMC) and its folder
    StartupStep_Preferences,     // the preferences read (config.json…)
    StartupStep_Borealis,        // aurore::Application::init: the platform, the translations
    StartupStep_Window,          // aurore::Application::createWindow: graphics (deko3d), sounds, fonts
    StartupStep_LoadingDrawn,    // the loading screen's first frame drawn
    StartupStep_Probed,          // app::init: what the app needs from the console probed
    StartupStep_Settings,        // the settings the first screen depends on read
    StartupStep_Intro,           // the intro over (Preferences › Show the intro; at once when off)
    StartupStep_FirstScreen,     // the first screen built and shown
    StartupStep_Count
} StartupStep;

// Marks `step` as ended now. Only the first call per step counts.
void startup_mark(StartupStep step);

// When `step` ended, in nanoseconds of the monotonic clock; false when it
// was never marked.
bool startup_time(StartupStep step, uint64_t* ns);

// Milliseconds from the end of `from` to the end of `to`; -1 when either
// was not marked.
long long startup_elapsed_ms(StartupStep from, StartupStep to);

// The report's section: one line per step reached, with its own time and
// the time since the first mark. `at_ns[i]` is when step i ended, or a
// negative value when it was not reached. Always NUL-terminated; returns
// the length the whole text needs (snprintf-like).
size_t startup_format(const int64_t at_ns[StartupStep_Count], char* out, size_t size);

// startup_format() of the steps marked so far.
size_t startup_report(char* out, size_t size);

#ifdef __cplusplus
}
#endif
