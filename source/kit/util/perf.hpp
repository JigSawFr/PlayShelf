// perf — how long the slow work of the screens takes on this console: the
// lists built, the tabs that refresh themselves, the installed games check.
// Each one keeps its count, average, longest and last time, shown in the
// diagnostic report's "timings" section, so a report from a console says
// where the time goes (the desktop simulation's figures are a PC's).
// Plain C++ so the host tests can run it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <string>

namespace perf
{

// Adds one run of `what` (a string literal: kept by its address) that took `ms`.
void record(const char* what, double ms);

// Times the scope it lives in as one run of `what`.
class Timed
{
public:
    explicit Timed(const char* what) : what_(what), start_(std::chrono::steady_clock::now()) {}
    ~Timed() { record(what_, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count()); }
    Timed(const Timed&) = delete;
    Timed& operator=(const Timed&) = delete;

private:
    const char* what_;
    std::chrono::steady_clock::time_point start_;
};

// The report's section: one line per kind of work, in the order first seen.
std::string report();

// Forgets every run (the tests).
void reset();

}   // namespace perf
