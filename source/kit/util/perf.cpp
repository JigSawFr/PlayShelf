// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/perf.hpp"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace perf
{

namespace
{
struct Entry
{
    const char* what;
    unsigned long count;
    double total_ms;
    double max_ms;
    double last_ms;
};

// Recorded from the UI thread and the background ones (the games check).
std::mutex s_lock;
std::vector<Entry> s_entries;
constexpr size_t MAX_ENTRIES = 32;   // a few kinds of work: a bound, not a limit met
}   // namespace

void record(const char* what, double ms)
{
    std::lock_guard<std::mutex> lock(s_lock);
    for (Entry& e : s_entries)
        if (e.what == what || std::strcmp(e.what, what) == 0) {
            e.count++;
            e.total_ms += ms;
            if (ms > e.max_ms) e.max_ms = ms;
            e.last_ms = ms;
            return;
        }
    if (s_entries.size() < MAX_ENTRIES) s_entries.push_back({ what, 1, ms, ms, ms });
}

std::string report()
{
    std::lock_guard<std::mutex> lock(s_lock);
    std::string out = "=== timings (ms) ===\n";
    if (s_entries.empty()) return out + "(none yet)\n";
    char line[160];
    std::snprintf(line, sizeof(line), "  %-28s %6s %9s %9s %9s\n", "", "runs", "average", "longest", "last");
    out += line;
    for (const Entry& e : s_entries) {
        std::snprintf(line, sizeof(line), "  %-28s %6lu %9.1f %9.1f %9.1f\n", e.what, e.count,
                      e.total_ms / (double)e.count, e.max_ms, e.last_ms);
        out += line;
    }
    return out;
}

void reset()
{
    std::lock_guard<std::mutex> lock(s_lock);
    s_entries.clear();
}

}   // namespace perf
