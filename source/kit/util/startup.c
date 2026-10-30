// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "startup.h"

#include <stdarg.h>
#include <stdio.h>

#ifdef __SWITCH__
#include <switch.h>
#else
#include <time.h>
#endif

// Written and read on the main thread only (userAppInit, main and the
// aurore::sync tasks). Zero-initialised: valid before the C++ constructors.
static uint64_t g_at[StartupStep_Count];
static bool g_marked[StartupStep_Count];

static uint64_t now_ns(void)
{
#ifdef __SWITCH__
    return armTicksToNs(armGetSystemTick());
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
#endif
}

void startup_mark(StartupStep step)
{
    if ((unsigned)step >= StartupStep_Count || g_marked[step]) return;
    g_at[step] = now_ns();
    g_marked[step] = true;
}

bool startup_time(StartupStep step, uint64_t* ns)
{
    if ((unsigned)step >= StartupStep_Count || !g_marked[step]) return false;
    if (ns) *ns = g_at[step];
    return true;
}

long long startup_elapsed_ms(StartupStep from, StartupStep to)
{
    uint64_t a, b;
    if (!startup_time(from, &a) || !startup_time(to, &b) || b < a) return -1;
    return (long long)((b - a) / 1000000u);
}

// What ended with each step; the first one reached is where the times start.
static const char* const LABELS[StartupStep_Count] = {
    "userAppInit called",
    "system services (userAppInit)",
    "C++ static initialisation",
    "data folder: older layouts",
    "session log, last run checked",
    "previous build check",
    "system: sysMMC or emuMMC",
    "preferences",
    "Aurore: platform, translations",
    "window: graphics, sounds, fonts",
    "loading screen, first frame",
    "app: console probed",
    "app: settings read",
    "intro (Preferences: Show the intro)",
    "first screen",
};

// snprintf at the end of what is written so far; `*len` counts what the
// whole text needs, even past `size`.
static void append(char* out, size_t size, size_t* len, const char* fmt, ...)
{
    const size_t room = *len < size ? size - *len : 0;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(room ? out + *len : NULL, room, fmt, ap);
    va_end(ap);
    if (n > 0) *len += (size_t)n;
}

static long long ms_between(int64_t from_ns, int64_t to_ns)
{
    return to_ns > from_ns ? (long long)((to_ns - from_ns) / 1000000) : 0;
}

size_t startup_format(const int64_t at_ns[StartupStep_Count], char* out, size_t size)
{
    size_t len = 0;
    if (size) out[0] = '\0';

    int first = -1;
    for (int i = 0; i < StartupStep_Count && first < 0; i++)
        if (at_ns[i] >= 0) first = i;
    if (first < 0) {
        append(out, size, &len, "start-up    : not measured\n");
        return len;
    }

    // The Switch starts at userAppInit, the desktop simulation at main().
    const char* from = first == StartupStep_ServicesBegin ? "userAppInit; hbloader's load of the .nro before it is not measured"
                       : first == StartupStep_Main        ? "main()"
                                                          : LABELS[first];
    if (at_ns[StartupStep_FirstScreen] >= 0)
        append(out, size, &len, "start-up    : first screen after %lld ms (from %s)\n",
               ms_between(at_ns[first], at_ns[StartupStep_FirstScreen]), from);
    else
        append(out, size, &len, "start-up    : first screen not reached (from %s)\n", from);
    append(out, size, &len, "  %-40s %6s %7s\n", "step", "ms", "total");

    int64_t prev = at_ns[first];
    for (int i = first + 1; i < StartupStep_Count; i++) {
        if (at_ns[i] < 0) continue;   // not reached, or not on this path (the init-error screen)
        append(out, size, &len, "  %-40s %6lld %7lld\n", LABELS[i], ms_between(prev, at_ns[i]),
               ms_between(at_ns[first], at_ns[i]));
        prev = at_ns[i];
    }
    return len;
}

size_t startup_report(char* out, size_t size)
{
    int64_t at_ns[StartupStep_Count];
    for (int i = 0; i < StartupStep_Count; i++)
        at_ns[i] = g_marked[i] ? (int64_t)g_at[i] : -1;
    return startup_format(at_ns, out, size);
}
