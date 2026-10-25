// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/date.hpp"

#include <cstdio>

#include "core/calendar.h"

namespace date
{

namespace
{
bool digits(const std::string& s, size_t from, size_t n)
{
    for (size_t i = from; i < from + n; i++)
        if (i >= s.size() || s[i] < '0' || s[i] > '9') return false;
    return true;
}

int number(const std::string& s, size_t from, size_t n)
{
    int v = 0;
    for (size_t i = from; i < from + n; i++) v = v * 10 + (s[i] - '0');
    return v;
}
}   // namespace

bool parse_shape(const std::string& s, Ymd* out)
{
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    if (!digits(s, 0, 4) || !digits(s, 5, 2) || !digits(s, 8, 2)) return false;
    if (out) *out = Ymd{ number(s, 0, 4), number(s, 5, 2), number(s, 8, 2) };
    return true;
}

bool parse_ranged(const std::string& s, Ymd* out)
{
    Ymd d;
    if (!parse_shape(s, &d) || d.month < 1 || d.month > 12 || d.day < 1 || d.day > 31) return false;
    if (out) *out = d;
    return true;
}

bool parse_valid(const std::string& s, Ymd* out)
{
    Ymd d;
    if (!parse_ranged(s, &d)) return false;
    const Ymd back = civil_from_days(days_from_civil(d));
    if (back.month != d.month || back.day != d.day) return false;   // 2026-02-30
    if (out) *out = d;
    return true;
}

int64_t days_from_civil(const Ymd& d)
{
    return calendar_days_from_civil(d.year, (unsigned)d.month, (unsigned)d.day);
}

Ymd civil_from_days(int64_t days)
{
    int y;
    unsigned m, d;
    calendar_civil_from_days(days, &y, &m, &d);
    return Ymd{ y, (int)m, (int)d };
}

std::string format(const Ymd& d)
{
    if (d.year < 0 || d.year > 9999) return "";
    char out[16];
    std::snprintf(out, sizeof(out), "%04d-%02d-%02d", d.year, d.month, d.day);
    return out;
}

bool day_number(const std::string& s, int64_t* out)
{
    Ymd d;
    if (!parse_valid(s, &d)) return false;
    *out = days_from_civil(d);
    return true;
}

std::string add_days(const std::string& s, int days)
{
    int64_t n;
    if (!day_number(s, &n)) return "";
    return format(civil_from_days(n + days));
}

bool days_between(const std::string& a, const std::string& b, int* days)
{
    Ymd da, db;
    if (!parse_ranged(a, &da) || !parse_ranged(b, &db)) return false;
    *days = (int)(days_from_civil(db) - days_from_civil(da));
    return true;
}

int weekday(const std::string& s)
{
    Ymd d;
    if (s.size() < 10 || !parse_ranged(s.substr(0, 10), &d)) return -1;
    return calendar_weekday(days_from_civil(d));
}

bool iso_seconds(const std::string& iso, int64_t* out)
{
    Ymd d;
    if (iso.size() < 19 || !parse_ranged(iso.substr(0, 10), &d)) return false;
    if (iso[10] != 'T' || iso[13] != ':' || iso[16] != ':') return false;
    if (!digits(iso, 11, 2) || !digits(iso, 14, 2) || !digits(iso, 17, 2)) return false;
    const int h = number(iso, 11, 2), mi = number(iso, 14, 2), se = number(iso, 17, 2);
    if (h > 23 || mi > 59 || se > 60) return false;
    *out = days_from_civil(d) * 86400 + h * 3600 + mi * 60 + se;
    return true;
}

int64_t days_between(int64_t from, int64_t to)
{
    const int64_t d = to - from;
    return d >= 0 ? d / 86400 : -((-d + 86399) / 86400);
}

}   // namespace date
