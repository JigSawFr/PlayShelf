// date — "YYYY-MM-DD" dates as the settings, the history and the notes keep
// them: reading one (three levels of strictness, each kept by a caller that
// relies on it), the day number since 1970-01-01, moving by days, the days
// between two, the weekday, an ISO 8601 UTC stamp as seconds, the days
// between two such times, and a time written in local time. The arithmetic
// is core/calendar.c's (one implementation for C and C++); this is its
// string side. Plain C++ so the host tests can run it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <ctime>
#include <string>

namespace date
{

struct Ymd
{
    int year = 0, month = 0, day = 0;
};

// Exactly "YYYY-MM-DD" with digits where they belong; the numbers as written,
// unchecked ("2026-13-45" passes). For a value that is only kept or compared.
bool parse_shape(const std::string& s, Ymd* out = nullptr);
// parse_shape, month 1..12 and day 1..31 ("2026-02-30" passes).
bool parse_ranged(const std::string& s, Ymd* out = nullptr);
// parse_ranged and a day the month has: a real calendar date.
bool parse_valid(const std::string& s, Ymd* out = nullptr);

// Days since 1970-01-01 of a proleptic Gregorian date, and back. The date's
// numbers are taken as they are (day 30 of February is March 1st or 2nd).
int64_t days_from_civil(const Ymd& d);
Ymd     civil_from_days(int64_t days);
// "2026-10-12"; "" outside years 0..9999.
std::string format(const Ymd& d);

// The day number of a real date (parse_valid); false when it is not one.
bool day_number(const std::string& s, int64_t* out);
// "2026-10-12" + 7 -> "2026-10-19" (months and leap years included);
// "" when `s` is not a real date or the result leaves years 0..9999.
std::string add_days(const std::string& s, int days);
// Days from `a` to `b` (parse_ranged); false if either is malformed.
bool days_between(const std::string& a, const std::string& b, int* days);
// 0 = Sunday, of the first ten characters of `s` (parse_ranged); -1 when they
// are no date.
int weekday(const std::string& s);

// Seconds since 1970 of "YYYY-MM-DDTHH:MM:SS…" (UTC, the rest ignored); false
// when malformed (fields out of range, a leap second allowed).
bool iso_seconds(const std::string& iso, int64_t* out);
// Whole days from `from` to `to` (seconds since 1970), rounded down: -1 when
// `to` is a second before `from`.
int64_t days_between(int64_t from, int64_t to);

// `t` in local time through strftime's `fmt` ("%H:%M:%S"), at most 31
// bytes; "?" when it cannot be written. Inline: session_log needs nothing
// else of this file (nor core/calendar.c).
inline std::string local_time(std::time_t t, const char* fmt)
{
    std::tm tmv{};
    char buf[32];
    if (!localtime_r(&t, &tmv) || std::strftime(buf, sizeof(buf), fmt, &tmv) == 0) return "?";
    return buf;
}

}   // namespace date
