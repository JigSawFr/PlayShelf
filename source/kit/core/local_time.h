// local_time — the console's user clock (what the HOME menu shows) in its
// time zone (Settings > Time Zone), read live.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once
#include "calendar.h"
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The console's time-zone rule, daylight saving included.
const TimeRule *time_console_rule(void);

// The user clock now, and the same in local time. False when the local time
// is unknown (`posix` is still set). Use this rather than time()/localtime():
// see calendar.h.
bool time_local_now(u64 *posix, LocalTime *local);

// Formats a POSIX time in the console's time zone ("2026-10-07 14:03:12").
// Falls back to UTC (with a " UTC" suffix) on failure. time_format_utc is in
// calendar.h.
void time_format_local(u64 posix, char *buf, size_t size);

#ifdef __cplusplus
}
#endif
