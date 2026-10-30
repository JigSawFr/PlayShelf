// Host tests for source/util/update.cpp: version comparison, firmware strings,
// compat.json parsing and the update decision of the firmware screen.
#include "check.h"
#include <cstdio>
#include <string>

#include "util/update.hpp"

static uint32_t fw(int a, int b, int c) { return ((uint32_t)a << 16) | ((uint32_t)b << 8) | (uint32_t)c; }

static void test_versions()
{
    int v[3];
    CHECK(update::parse_version("1.2.3", v) && v[0] == 1 && v[1] == 2 && v[2] == 3);
    CHECK(update::parse_version("v10.0.42", v) && v[0] == 10 && v[2] == 42);
    CHECK(update::parse_version("0.0.0-dev", v) && v[0] == 0);
    CHECK(!update::parse_version("", v));
    CHECK(!update::parse_version("1.2", v));
    CHECK(!update::parse_version("1..3", v));
    CHECK(!update::parse_version("1.2.3.4", v));
    CHECK(!update::parse_version("1.2.x", v));
    CHECK(!update::parse_version("1.2.99999", v));

    CHECK(update::compare_versions("1.0.0", "1.0.0") == 0);
    CHECK(update::compare_versions("1.0.1", "1.0.0") > 0);
    CHECK(update::compare_versions("1.10.0", "1.9.9") > 0);    // numeric, not text
    CHECK(update::compare_versions("2.0.0", "10.0.0") < 0);
    CHECK(update::compare_versions("v1.2.0", "1.2.0") == 0);
    CHECK(update::compare_versions("1.0.0-dev", "1.0.0") < 0);
    CHECK(update::compare_versions("1.6.2.3", "1.6.2.1") > 0);   // sys-patch's fourth number
    CHECK(update::compare_versions("1.6.2.0", "1.6.2") == 0);
    CHECK(update::compare_versions("1.6.3", "1.6.2.3") > 0);
    CHECK(update::compare_versions("1.6.2.", "1.6.2") < 0);    // not a version
    CHECK(update::compare_versions("1.0.0", "0.0.0-dev") > 0);
    CHECK(update::compare_versions("garbage", "0.0.1") < 0);
    CHECK(update::compare_versions("0.0.1", "garbage") > 0);

    CHECK(update::parse_firmware("24.0.0") == fw(24, 0, 0));
    CHECK(update::parse_firmware("23.0.1") == fw(23, 0, 1));
    CHECK(update::parse_firmware("256.0.0") == 0);
    CHECK(update::parse_firmware("23.0") == 0);
    CHECK(update::parse_firmware("23.0.1-rc") == 0);
}

static void test_compat()
{
    update::Latest l;
    CHECK(update::parse_compat(
        R"({"schema": 1, "version": "1.1.0", "fw_tested_max": "24.0.0", "fw_min_play_timer": "21.0.0"})", &l));
    CHECK(l.version == "1.1.0" && l.fw_tested_max == fw(24, 0, 0));
    // Unknown fields are ignored (a later schema may add some).
    CHECK(update::parse_compat(R"({"version": "2.0.0", "fw_tested_max": "25.1.0", "notes": [1, 2]})", &l));
    CHECK(l.version == "2.0.0" && l.fw_tested_max == fw(25, 1, 0));

    // The schema: missing is 1; a newer one is not read (the release's
    // compat.json, in schema 1, is fetched instead).
    CHECK(update::parse_compat(R"({"schema": 1, "version": "1.2.0", "fw_tested_max": "24.0.0"})", &l));
    CHECK(update::COMPAT_SCHEMA == 1);

    update::Latest untouched;
    untouched.version = "kept";
    const char* bad[] = {
        "",
        "not json",
        "[]",
        R"({"version": "1.1.0"})",
        R"({"fw_tested_max": "24.0.0"})",
        R"({"version": 110, "fw_tested_max": "24.0.0"})",
        R"({"version": "1.1", "fw_tested_max": "24.0.0"})",
        R"({"version": "1.1.0", "fw_tested_max": "24"})",
        R"({"version": "1.1.0", "fw_tested_max": 24})",
        "<html>404</html>",
        R"({"schema": 2, "version": "3.0.0", "fw_tested_max": "26.0.0"})",
    };
    for (const char* text : bad) {
        CHECK(!update::parse_compat(text, &untouched));
        CHECK(untouched.version == "kept");
    }
}

static void test_decide()
{
    update::Latest l;
    l.version       = "1.1.0";
    l.fw_tested_max = fw(24, 0, 0);
    using update::Verdict;
    CHECK(update::decide(fw(24, 0, 0), "1.0.0", l) == Verdict::UpdateSupports);
    CHECK(update::decide(fw(23, 0, 1), "1.0.0", l) == Verdict::UpdateSupports);
    CHECK(update::decide(fw(24, 0, 1), "1.0.0", l) == Verdict::UpdateNoSupport);
    CHECK(update::decide(fw(25, 0, 0), "1.0.0", l) == Verdict::UpdateNoSupport);
    CHECK(update::decide(fw(25, 0, 0), "1.1.0", l) == Verdict::UpToDate);
    CHECK(update::decide(fw(24, 0, 0), "1.2.0", l) == Verdict::UpToDate);       // newer than the release
    CHECK(update::decide(fw(24, 0, 0), "0.0.0-dev", l) == Verdict::UpdateSupports);
    CHECK(update::decide(0, "1.0.0", l) == Verdict::UpdateNoSupport);           // unknown console firmware
}

static void test_snooze()
{
    const update::Snooze none;
    CHECK(update::offer_at_start("1.2.0", "1.1.0", none, "2026-10-12"));
    CHECK(!update::offer_at_start("1.1.0", "1.1.0", none, "2026-10-12"));    // already running it
    CHECK(!update::offer_at_start("1.0.0", "1.1.0", none, "2026-10-12"));
    CHECK(!update::offer_at_start("", "1.1.0", none, "2026-10-12"));         // never checked
    CHECK(!update::offer_at_start("garbage", "1.1.0", none, "2026-10-12"));
    CHECK(update::offer_at_start("1.0.0", "1.0.0-dev", none, "2026-10-12"));   // a dev build of it

    const update::Snooze week{ "1.2.0", "2026-10-19" };
    CHECK(!update::offer_at_start("1.2.0", "1.1.0", week, "2026-10-12"));
    CHECK(!update::offer_at_start("1.2.0", "1.1.0", week, "2026-10-18"));
    CHECK(update::offer_at_start("1.2.0", "1.1.0", week, "2026-10-19"));      // the day it ends
    CHECK(update::offer_at_start("1.3.0", "1.1.0", week, "2026-10-12"));      // a newer release
    CHECK(!update::offer_at_start("1.2.0", "1.1.0", { "1.2.0", "2026-11-11" }, "2026-10-12"));   // 30 days
    CHECK(update::offer_at_start("1.2.0", "1.1.0", { "1.2.0", "2026-11-12" }, "2026-10-12"));   // 31: ignored
    CHECK(update::offer_at_start("1.2.0", "1.1.0", { "1.2.0", "2027-10-12" }, "2026-10-12"));   // clock set back
    CHECK(update::offer_at_start("1.2.0", "1.1.0", { "1.2.0", "bad" }, "2026-10-12"));
    CHECK(update::offer_at_start("1.2.0", "1.1.0", week, ""));
}

int main()
{
    test_versions();
    test_compat();
    test_decide();
    test_snooze();
    return CHECK_DONE("update version, compat.json, decision and postponement assertions passed");
}
