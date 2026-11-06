// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/cpu_boost.hpp"

#include <mutex>

#include "core/boost.h"

namespace cpu_boost
{
namespace
{
std::mutex s_mutex;
int        s_count = 0;
}   // namespace

void acquire()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_count++ == 0) core_cpu_boost(true);
}

void release()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_count == 0) return;   // more releases than acquires: ignored
    if (--s_count == 0) core_cpu_boost(false);
}

void release_all()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_count == 0) return;
    s_count = 0;
    core_cpu_boost(false);
}

int holders()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_count;
}
}   // namespace cpu_boost
