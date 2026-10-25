// Simulated core/local_time.h: the host's clock and time zone standing in for
// the console's (TZ= changes it); PLAYSHELF_SIM_NOW (POSIX seconds) freezes
// it, for screenshots that do not change from one run to the next.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../core/local_time.h"
#include "../core/sim_env.h"

static u64 sim_now(void)
{
    const long long frozen = sim_env_int("PLAYSHELF_SIM_NOW", 0, 0, LLONG_MAX);
    return frozen > 0 ? (u64)frozen : (u64)time(NULL);
}

static bool host_to_local(void *ctx, u64 posix, LocalTime *out)
{
    (void)ctx;
    time_t t = (time_t)posix;
    struct tm tmv;
    if (!localtime_r(&t, &tmv)) return false;
    out->year = (u16)(tmv.tm_year + 1900); out->month = (u8)(tmv.tm_mon + 1); out->day = (u8)tmv.tm_mday;
    out->hour = (u8)tmv.tm_hour; out->minute = (u8)tmv.tm_min; out->second = (u8)tmv.tm_sec;
    out->wday = (u8)tmv.tm_wday;
    return true;
}

static int host_to_posix(void *ctx, const LocalTime *w, u64 out[2])
{
    (void)ctx;
    int n = 0;
    for (int dst = 0; dst <= 1; dst++) {   // both readings of a repeated hour
        struct tm tmv;
        memset(&tmv, 0, sizeof(tmv));
        tmv.tm_year = w->year - 1900; tmv.tm_mon = w->month - 1; tmv.tm_mday = w->day;
        tmv.tm_hour = w->hour; tmv.tm_min = w->minute; tmv.tm_sec = w->second;
        tmv.tm_isdst = dst;
        const time_t t = mktime(&tmv);
        LocalTime back;
        if (t == (time_t)-1 || !host_to_local(NULL, (u64)t, &back)) continue;
        if (back.day != w->day || back.hour != w->hour || back.minute != w->minute) continue;   // in a gap
        if (n == 0 || out[0] != (u64)t) out[n++] = (u64)t;
    }
    if (n == 2 && out[1] < out[0]) { u64 x = out[0]; out[0] = out[1]; out[1] = x; }
    return n;
}

const TimeRule *time_console_rule(void)
{
    static const TimeRule rule = { host_to_local, host_to_posix, NULL };
    return &rule;
}

bool time_local_now(u64 *posix, LocalTime *local)
{
    const u64 now = sim_now();
    if (posix) *posix = now;
    return local ? host_to_local(NULL, now, local) : true;
}

void time_format_local(u64 posix, char *buf, size_t size)
{
    time_t t = (time_t)posix; struct tm tmv;
    if (!localtime_r(&t, &tmv) || !strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tmv)) time_format_utc(posix, buf, size);
}
