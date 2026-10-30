// Host tests for source/util/startup.c: the start-up times of the
// diagnostic report.
#include "check.h"
#include <string.h>

#include "startup.h"

enum { MS = 1000000 };

// A console start: every step reached, from userAppInit.
static void console_times(int64_t at[StartupStep_Count])
{
    static const int ms[StartupStep_Count] = { 0, 412, 415, 416, 418, 419, 420, 423, 443, 823, 863, 983, 993, 2863, 2913 };
    const int64_t boot = 3600LL * 1000 * MS;   // the system tick counts from the console's boot
    for (int i = 0; i < StartupStep_Count; i++) at[i] = boot + (int64_t)ms[i] * MS;
}

static void test_console(void)
{
    int64_t at[StartupStep_Count];
    console_times(at);
    char out[2048];
    const size_t len = startup_format(at, out, sizeof(out));
    CHECK(len == strlen(out));
    CHECK(strstr(out, "start-up    : first screen after 2913 ms (from userAppInit; hbloader's load of the .nro "
                      "before it is not measured)\n") == out);
    // Each step's own time, then the time since the first mark.
    CHECK(strstr(out, "  system services (userAppInit)               412     412\n"));
    CHECK(strstr(out, "  C++ static initialisation                     3     415\n"));
    CHECK(strstr(out, "  window: graphics, sounds, fonts             380     823\n"));
    CHECK(strstr(out, "  intro (Preferences: Show the intro)        1870    2863\n"));   // the intro: 2 s from the loading screen
    CHECK(strstr(out, "  first screen                                 50    2913\n"));
    // The first mark is where the times start, not a step.
    CHECK(!strstr(out, "userAppInit called"));
    // One line per step reached after the first, the header and the column titles.
    int lines = 0;
    for (const char* p = out; *p; p++) lines += *p == '\n';
    CHECK(lines == 2 + StartupStep_Count - 1);
}

static void test_desktop(void)
{
    // The desktop simulation has no userAppInit: the times start at main().
    int64_t at[StartupStep_Count];
    console_times(at);
    at[StartupStep_ServicesBegin] = at[StartupStep_Services] = -1;
    char out[2048];
    startup_format(at, out, sizeof(out));
    CHECK(strstr(out, "start-up    : first screen after 2498 ms (from main())\n") == out);
    CHECK(!strstr(out, "system services"));
    CHECK(!strstr(out, "C++ static initialisation"));
    CHECK(strstr(out, "  data folder: older layouts                    1       1\n"));
    CHECK(strstr(out, "  preferences                                   3       8\n"));
}

static void test_missing_steps(void)
{
    // The init-error screen: no settings step, no intro, and the first
    // screen's time counts from the probe.
    int64_t at[StartupStep_Count];
    console_times(at);
    at[StartupStep_Settings] = at[StartupStep_Intro] = -1;
    at[StartupStep_FirstScreen] = at[StartupStep_Probed] + 50LL * MS;
    char out[2048];
    startup_format(at, out, sizeof(out));
    CHECK(!strstr(out, "app: settings read") && !strstr(out, "intro"));
    CHECK(strstr(out, "  first screen                                 50    1033\n"));

    // Not there yet.
    console_times(at);
    at[StartupStep_FirstScreen] = -1;
    startup_format(at, out, sizeof(out));
    CHECK(strstr(out, "start-up    : first screen not reached (from userAppInit;") == out);
    CHECK(!strstr(out, "  first screen "));

    // Nothing marked.
    for (int i = 0; i < StartupStep_Count; i++) at[i] = -1;
    CHECK(startup_format(at, out, sizeof(out)) == strlen("start-up    : not measured\n"));
    CHECK(strcmp(out, "start-up    : not measured\n") == 0);

    // A step marked before the previous one (should not happen) reads 0, not
    // a huge number.
    console_times(at);
    at[StartupStep_Main] = at[StartupStep_ServicesBegin];
    startup_format(at, out, sizeof(out));
    CHECK(strstr(out, "  C++ static initialisation                     0       0\n"));
    CHECK(strstr(out, "  data folder: older layouts                  416     416\n"));
}

static void test_truncated(void)
{
    int64_t at[StartupStep_Count];
    console_times(at);
    char full[2048];
    const size_t need = startup_format(at, full, sizeof(full));

    // A small buffer: what fits, NUL-terminated, and the length it needs.
    char small[64];
    memset(small, 'x', sizeof(small));
    CHECK(startup_format(at, small, sizeof(small)) == need);
    CHECK(strlen(small) == sizeof(small) - 1);
    CHECK(strncmp(small, full, sizeof(small) - 1) == 0);

    // No buffer at all: only the length.
    CHECK(startup_format(at, NULL, 0) == need);
}

static void test_marks(void)
{
    uint64_t a = 0, b = 0;
    CHECK(!startup_time(StartupStep_Main, &a));
    CHECK(startup_elapsed_ms(StartupStep_Main, StartupStep_Window) == -1);
    char out[2048];
    startup_report(out, sizeof(out));
    CHECK(strcmp(out, "start-up    : not measured\n") == 0);

    startup_mark(StartupStep_Main);
    REQUIRE(startup_time(StartupStep_Main, &a));
    // Only the first mark counts.
    startup_mark(StartupStep_Main);
    CHECK(startup_time(StartupStep_Main, &b) && b == a);
    CHECK(startup_time(StartupStep_Main, NULL));

    startup_mark(StartupStep_Window);
    CHECK(startup_time(StartupStep_Window, &b) && b >= a);
    CHECK(startup_elapsed_ms(StartupStep_Main, StartupStep_Window) >= 0);
    CHECK(startup_elapsed_ms(StartupStep_Main, StartupStep_FirstScreen) == -1);

    // Out of range: ignored.
    startup_mark(StartupStep_Count);
    CHECK(!startup_time(StartupStep_Count, &a));

    startup_report(out, sizeof(out));
    CHECK(strstr(out, "start-up    : first screen not reached (from main())\n") == out);
    CHECK(strstr(out, "  window: graphics, sounds, fonts "));
}

int main(void)
{
    test_console();
    test_desktop();
    test_missing_steps();
    test_truncated();
    test_marks();
    return CHECK_DONE("startup: all tests passed");
}
