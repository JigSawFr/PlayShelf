// Host tests for source/util/perf.cpp: the diagnostic report's timings.
#include "check.h"
#include <cstdio>
#include <string>
#include <thread>

#include "util/perf.hpp"

static void test_report()
{
    perf::reset();
    CHECK(perf::report() == "=== timings (ms) ===\n(none yet)\n");
    perf::record("history list", 180.0);
    perf::record("Overview refresh", 12.5);
    perf::record("history list", 20.0);
    // The same text from another literal is the same kind of work.
    const std::string same = "Overview refresh";
    perf::record(same.c_str(), 7.5);
    const std::string r = perf::report();
    CHECK(r.find("=== timings (ms) ===\n") == 0);
    // In the order first seen; the average, the longest and the last.
    CHECK(r.find("  history list                      2     100.0     180.0      20.0\n") != std::string::npos);
    CHECK(r.find("  Overview refresh                  2      10.0      12.5       7.5\n") != std::string::npos);
    CHECK(r.find("history list") < r.find("Overview refresh"));
}

static void test_timed()
{
    perf::reset();
    {
        perf::Timed t("a scope");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    const std::string r = perf::report();
    const size_t at = r.find("  a scope ");
    REQUIRE(at != std::string::npos);
    double avg = 0;
    unsigned long runs = 0;
    CHECK(std::sscanf(r.c_str() + at, "  a scope %lu %lf", &runs, &avg) == 2);
    CHECK(runs == 1 && avg >= 19.0 && avg < 1000.0);
}

static void test_bounded()
{
    perf::reset();
    static const char* const names[40] = {
        "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7", "w8", "w9", "w10", "w11", "w12", "w13",
        "w14", "w15", "w16", "w17", "w18", "w19", "w20", "w21", "w22", "w23", "w24", "w25", "w26", "w27",
        "w28", "w29", "w30", "w31", "w32", "w33", "w34", "w35", "w36", "w37", "w38", "w39" };
    for (const char* n : names) perf::record(n, 1.0);
    const std::string r = perf::report();
    CHECK(r.find("  w31 ") != std::string::npos && r.find("  w32 ") == std::string::npos);
}

int main()
{
    test_report();
    test_timed();
    test_bounded();
    return CHECK_DONE("timings: averages, longest, last, scopes and the bound passed");
}
