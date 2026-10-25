// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "local_time.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static bool console_to_local(void *ctx, u64 posix, LocalTime *out)
{
    (void)ctx;
    TimeCalendarTime cal;
    TimeCalendarAdditionalInfo info;
    if (R_FAILED(timeToCalendarTimeWithMyRule(posix, &cal, &info)) || info.wday > 6) return false;
    out->year = cal.year;
    out->month = cal.month;
    out->day = cal.day;
    out->hour = cal.hour;
    out->minute = cal.minute;
    out->second = cal.second;
    out->wday = (u8)info.wday;
    return true;
}

static int console_to_posix(void *ctx, const LocalTime *wall, u64 out[2])
{
    (void)ctx;
    TimeCalendarTime cal;
    memset(&cal, 0, sizeof(cal));
    cal.year = wall->year;
    cal.month = wall->month;
    cal.day = wall->day;
    cal.hour = wall->hour;
    cal.minute = wall->minute;
    cal.second = wall->second;
    s32 count = 0;
    if (R_FAILED(timeToPosixTimeWithMyRule(&cal, out, 2, &count)) || count < 0) return 0;
    return count > 2 ? 2 : count;
}

const TimeRule *time_console_rule(void)
{
    static const TimeRule rule = { console_to_local, console_to_posix, NULL };
    return &rule;
}

bool time_local_now(u64 *posix, LocalTime *local)
{
    u64 now = 0;
    if (R_FAILED(timeGetCurrentTime(TimeType_UserSystemClock, &now))) now = (u64)time(NULL);
    if (posix) *posix = now;
    return local ? console_to_local(NULL, now, local) : true;
}

void time_format_local(u64 posix, char *buf, size_t size)
{
    TimeCalendarTime cal;
    TimeCalendarAdditionalInfo info;
    if (R_SUCCEEDED(timeToCalendarTimeWithMyRule(posix, &cal, &info))) {
        snprintf(buf, size, "%04u-%02u-%02u %02u:%02u:%02u", (unsigned)cal.year, (unsigned)cal.month,
                 (unsigned)cal.day, (unsigned)cal.hour, (unsigned)cal.minute, (unsigned)cal.second);
        return;
    }
    time_format_utc(posix, buf, size);
}
