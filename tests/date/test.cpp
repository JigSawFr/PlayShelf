// Host tests for source/util/date.cpp: reading "YYYY-MM-DD" at its three
// levels of strictness, day numbers, moving by days, the days between two
// dates, the weekday, ISO 8601 stamps as seconds, the days between two
// such times and a time written in local time.
#include "check.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "util/date.hpp"

static void test_parse()
{
    date::Ymd d;
    CHECK(date::parse_shape("2026-10-16", &d) && d.year == 2026 && d.month == 10 && d.day == 16);
    CHECK(date::parse_shape("2026-13-45", &d) && d.month == 13 && d.day == 45);   // shape only
    CHECK(date::parse_shape("0000-00-00"));
    CHECK(!date::parse_shape("2026-1-016") && !date::parse_shape("2026/10/16") && !date::parse_shape(""));
    CHECK(!date::parse_shape("2026-10-16T10:00") && !date::parse_shape("2026-10-1a") && !date::parse_shape("+026-10-16"));

    CHECK(date::parse_ranged("2026-02-30", &d) && d.day == 30);   // in range, not a real day
    CHECK(date::parse_ranged("2026-12-31"));
    CHECK(!date::parse_ranged("2026-13-01") && !date::parse_ranged("2026-00-10"));
    CHECK(!date::parse_ranged("2026-10-00") && !date::parse_ranged("2026-10-32"));

    CHECK(date::parse_valid("2028-02-29", &d) && d.month == 2 && d.day == 29);   // leap year
    CHECK(date::parse_valid("2000-02-29"));                                      // divisible by 400
    CHECK(!date::parse_valid("2100-02-29") && !date::parse_valid("2026-02-29"));
    CHECK(!date::parse_valid("2026-02-30") && !date::parse_valid("2026-04-31"));
    CHECK(!date::parse_valid("2026-13-01"));
}

static void test_days()
{
    CHECK(date::days_from_civil({ 1970, 1, 1 }) == 0);
    CHECK(date::days_from_civil({ 2026, 10, 16 }) == 20742);
    CHECK(date::days_from_civil({ 1969, 12, 31 }) == -1);
    const date::Ymd back = date::civil_from_days(20742);
    CHECK(back.year == 2026 && back.month == 10 && back.day == 16);
    CHECK(date::format({ 2026, 3, 1 }) == "2026-03-01" && date::format({ -1, 1, 1 }).empty());
    CHECK(date::format({ 10000, 1, 1 }).empty());

    int64_t n = 0;
    CHECK(date::day_number("2026-10-16", &n) && n == 20742);
    CHECK(!date::day_number("2026-02-30", &n) && !date::day_number("", &n));

    CHECK(date::add_days("2026-10-12", 7) == "2026-10-19");
    CHECK(date::add_days("2026-10-12", 30) == "2026-11-11");
    CHECK(date::add_days("2026-12-25", 10) == "2027-01-04");
    CHECK(date::add_days("2028-02-28", 1) == "2028-02-29");   // a leap year
    CHECK(date::add_days("2026-02-28", 1) == "2026-03-01");
    CHECK(date::add_days("", 1).empty());
    CHECK(date::add_days("2026-03-01", -1) == "2026-02-28");
    CHECK(date::add_days("2026-02-30", 1).empty() && date::add_days("2026-1-01", 1).empty());
    CHECK(date::add_days("9999-12-31", 1).empty() && date::add_days("0000-01-01", -1).empty());

    int days = 0;
    CHECK(date::days_between("2026-10-09", "2026-11-08", &days) && days == 30);
    CHECK(date::days_between("2026-12-31", "2027-01-01", &days) && days == 1);
    CHECK(date::days_between("2028-02-28", "2028-03-01", &days) && days == 2);   // leap year
    CHECK(date::days_between("2026-10-09", "2026-10-01", &days) && days == -8);
    CHECK(!date::days_between("", "2026-10-09", &days));
    CHECK(!date::days_between("2026-13-01", "2026-10-09", &days));
    CHECK(!date::days_between("2026/10/09", "2026-10-09", &days));
}

static void test_weekday()
{
    CHECK(date::weekday("2026-10-08") == 4);   // a Thursday
    CHECK(date::weekday("2026-10-11") == 0);   // a Sunday
    CHECK(date::weekday("2026-10-11 18:30") == 0);   // a stamp: its date
    CHECK(date::weekday("2024-02-29") == 4);
    CHECK(date::weekday("2000-01-01") == 6);
    CHECK(date::weekday("1969-12-31") == 3);
    CHECK(date::weekday("2026-13-01") == -1 && date::weekday("2026-1-01") == -1 && date::weekday("") == -1);
}

static void test_iso()
{
    int64_t s = 0;
    CHECK(date::iso_seconds("1970-01-01T00:00:00Z", &s) && s == 0);
    CHECK(date::iso_seconds("2026-10-16T00:05:51Z", &s) && s == 20742LL * 86400 + 351);
    CHECK(date::iso_seconds("2026-10-16T23:59:60", &s) && s == 20742LL * 86400 + 86400);   // a leap second
    CHECK(!date::iso_seconds("2026-10-16", &s) && !date::iso_seconds("2026-10-16 00:05:51Z", &s));
    CHECK(!date::iso_seconds("2026-10-16T24:00:00Z", &s) && !date::iso_seconds("2026-10-16T10:60:00Z", &s));
    CHECK(!date::iso_seconds("2026-13-16T10:00:00Z", &s) && !date::iso_seconds("2026-10-16T1a:00:00Z", &s));
}

static void test_seconds()
{
    int64_t a = 0, b = 0;
    CHECK(date::iso_seconds("2026-10-01T12:00:00Z", &a));
    CHECK(date::iso_seconds("2026-10-04T11:59:59Z", &b) && date::days_between(a, b) == 2);
    CHECK(date::iso_seconds("2026-10-04T12:00:00Z", &b) && date::days_between(a, b) == 3);
    CHECK(date::iso_seconds("2026-12-31T00:00:00Z", &a) && date::iso_seconds("2027-03-01T00:00:00Z", &b) &&
          date::days_between(a, b) == 60);
    CHECK(date::iso_seconds("2028-02-28T00:00:00Z", &a) && date::iso_seconds("2028-03-01T00:00:00Z", &b) &&
          date::days_between(a, b) == 2);   // a leap year
    CHECK(date::days_between(0, 0) == 0 && date::days_between(0, 86399) == 0 && date::days_between(0, 86400) == 1);
    CHECK(date::days_between(0, -1) == -1 && date::days_between(0, -86400) == -1 && date::days_between(0, -86401) == -2);

    // In UTC, so the same everywhere.
    setenv("TZ", "UTC", 1);
    tzset();
    CHECK(date::local_time(0, "%Y-%m-%d %H:%M:%S") == "1970-01-01 00:00:00");
    CHECK(date::local_time(20742LL * 86400 + 351, "%H:%M:%S") == "00:05:51");
    CHECK(date::local_time(0, "%Y-%m-%d %H:%M:%S and %Y-%m-%d") == "?");   // past 31 bytes
    CHECK(date::local_time(0, "") == "?");
}

int main()
{
    test_parse();
    test_days();
    test_weekday();
    test_iso();
    test_seconds();
    CHECK_DONE("date parsing, day number, add, between, weekday, ISO stamp and local time assertions passed");
}
