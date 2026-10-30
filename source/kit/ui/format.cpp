// ui — the console's local date and time, and a moment, as text (limits,
// play time and names: format_text.cpp).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"
#include "core/local_time.h"

#include <ctime>
#include <format>
#include <mutex>
#include <thread>

namespace ui
{

namespace
{
// SameTime: the thread it pins the time for, and that time once read.
std::mutex      s_same_lock;
std::thread::id s_same_thread;
bool            s_same_read = false;
LocalTime       s_same_time{};

LocalTime read_local_now()
{
    LocalTime l{};
    u64 posix = 0;
    if (time_local_now(&posix, &l)) return l;
    // No time-zone rule: the C library's idea of local time.
    std::time_t now = (std::time_t)posix;
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    l.year = (uint16_t)(tmv.tm_year + 1900);
    l.month = (uint8_t)(tmv.tm_mon + 1);
    l.day = (uint8_t)tmv.tm_mday;
    l.hour = (uint8_t)tmv.tm_hour;
    l.minute = (uint8_t)tmv.tm_min;
    l.second = (uint8_t)tmv.tm_sec;
    l.wday = (uint8_t)tmv.tm_wday;
    return l;
}
}   // namespace

SameTime::SameTime()
{
    std::lock_guard<std::mutex> lock(s_same_lock);
    outer_ = s_same_thread == std::thread::id();
    if (!outer_) return;
    s_same_thread = std::this_thread::get_id();
    s_same_read = false;
}

SameTime::~SameTime()
{
    if (!outer_) return;
    std::lock_guard<std::mutex> lock(s_same_lock);
    s_same_thread = std::thread::id();
}

LocalTime local_now()
{
    {
        std::lock_guard<std::mutex> lock(s_same_lock);
        if (s_same_thread == std::this_thread::get_id() && s_same_read) return s_same_time;
    }
    const LocalTime l = read_local_now();
    std::lock_guard<std::mutex> lock(s_same_lock);
    if (s_same_thread == std::this_thread::get_id()) {
        s_same_time = l;
        s_same_read = true;
    }
    return l;
}

int today_weekday()
{
    return local_now().wday;
}

std::string today_date()
{
    const LocalTime l = local_now();
    return std::format("{:04d}-{:02d}-{:02d}", (int)l.year, (int)l.month, (int)l.day);
}

std::string now_hms()
{
    const LocalTime l = local_now();
    return std::format("{:02d}:{:02d}:{:02d}", (int)l.hour, (int)l.minute, (int)l.second);
}

std::string now_stamp()
{
    const LocalTime l = local_now();
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}", (int)l.year, (int)l.month, (int)l.day, (int)l.hour, (int)l.minute);
}

std::string time_text(uint64_t posix)
{
    if (!posix) return "—";
    char buf[48];
    time_format_local(posix, buf, sizeof(buf));
    return buf;
}

}   // namespace ui
