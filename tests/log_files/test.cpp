// Host tests for source/kit/util/log_files.cpp: the names of the reports,
// crash and upload files, their order, a new one, what the limits remove (by
// count, age and size, any of them; never the newest).
#include "check.h"
#include "fs.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "util/log_files.hpp"
#include "util/paths.hpp"

namespace lf = log_files;

namespace
{
std::string g_root;

void write(const std::string& rel, const std::string& content)
{
    fs::write(g_root + "/" + rel, content);
}

void write_size(const std::string& rel, size_t size)
{
    write(rel, std::string(size, 'x'));
}

bool there(const std::string& rel)
{
    return fs::exists(g_root + "/" + rel);
}

std::string read(const std::string& rel)
{
    return fs::read(g_root + "/" + rel);
}

// 2026-10-11 12:00:00 UTC (TZ is UTC in main()).
constexpr std::time_t NOW = 1791720000;
}   // namespace

static void test_names()
{
    CHECK(lf::name(lf::REPORT, "20261011_142530") == "report_20261011_142530.txt");
    CHECK(lf::name(lf::CRASH, "20261011_142530", 2) == "crash_20261011_142530_2.txt");
    CHECK(lf::name(lf::UPLOAD, "20261011_142530") == "upload_20261011_142530.url");
    CHECK(lf::name(lf::SESSION, "20261011_142530") == "session_20261011_142530.txt");
    CHECK(lf::stamp(NOW) == "20261011_120000");
    CHECK(lf::stamp(0) == "19700101_000000");

    std::string st;
    CHECK(lf::parse("report_20261011_142530.txt", lf::REPORT, &st) && st == "20261011_142530");
    CHECK(lf::parse("report_20261011_142530_12.txt", lf::REPORT, &st) && st == "20261011_142530");
    CHECK(!lf::parse("report_20261011_142530.txt", lf::CRASH));
    CHECK(!lf::parse("report_20261011_142530_.txt", lf::REPORT));
    CHECK(!lf::parse("report_20261011_142530_x.txt", lf::REPORT));
    CHECK(!lf::parse("report_2026101_142530.txt", lf::REPORT));
    CHECK(!lf::parse("report_20261011-142530.txt", lf::REPORT));
    CHECK(!lf::parse("report_20261011_142530.csv", lf::REPORT));
    CHECK(!lf::parse("20261011_142530.txt", lf::REPORT));
    CHECK(!lf::parse("reports_20261011_142530.txt", lf::REPORT));
}

static void test_list()
{
    const TempDir root("logs");
    g_root = root.path();
    write("r/report_20261009_141203.txt", "a");
    write("r/report_20261009_141203_1.txt", "b");
    write("r/report_20261009_141203_10.txt", "c");
    write("r/report_20261009_141203_2.txt", "d");
    write("r/report_20261011_080000.txt", "e");
    write("r/report_20261001_080000.txt", "f");
    write("r/notes.txt", "not a report");
    write("r/crash_20261012_080000.txt", "not a report");
    const auto names = lf::list(g_root + "/r", lf::REPORT);
    if (CHECK(names.size() == 6)) {
        CHECK(names[0] == "report_20261011_080000.txt");
        // In the same second: _10 after _2, the one without a number last.
        CHECK(names[1] == "report_20261009_141203_10.txt");
        CHECK(names[2] == "report_20261009_141203_2.txt");
        CHECK(names[3] == "report_20261009_141203_1.txt");
        CHECK(names[4] == "report_20261009_141203.txt");
        CHECK(names[5] == "report_20261001_080000.txt");
    }
    CHECK(lf::list(g_root + "/missing", lf::REPORT).empty());
}

static void test_to_prune()
{
    const std::vector<lf::File> files = {
        { "f5", "20261011_100000", 400 * 1024 },
        { "f4", "20261010_100000", 400 * 1024 },
        { "f3", "20261005_100000", 400 * 1024 },
        { "f2", "20260901_100000", 400 * 1024 },
        { "f1", "20250101_100000", 400 * 1024 },
    };
    const std::string cutoff = "20261004_120000";   // 7 days before NOW

    CHECK(lf::to_prune(files, lf::Limits{}, cutoff).empty());
    CHECK((lf::to_prune(files, lf::Limits{ 3, 0, 0 }, cutoff) == std::vector<std::string>{ "f2", "f1" }));
    CHECK((lf::to_prune(files, lf::Limits{ 0, 7, 0 }, cutoff) == std::vector<std::string>{ "f2", "f1" }));
    // 1 MB: the first two (800 KB) fit, the third goes over.
    CHECK((lf::to_prune(files, lf::Limits{ 0, 0, 1 }, cutoff) == std::vector<std::string>{ "f3", "f2", "f1" }));
    // Any limit removes: the count keeps 4, the size keeps 2.
    CHECK((lf::to_prune(files, lf::Limits{ 4, 0, 1 }, cutoff) == std::vector<std::string>{ "f3", "f2", "f1" }));
    CHECK((lf::to_prune(files, lf::Limits{ 1, 0, 0 }, cutoff) == std::vector<std::string>{ "f4", "f3", "f2", "f1" }));

    // The newest never goes: not when too old, not when too big alone.
    const std::vector<lf::File> old = { { "big", "20200101_000000", 5 * 1024 * 1024 }, { "older", "20190101_000000", 1 } };
    CHECK((lf::to_prune(old, lf::Limits{ 0, 7, 0 }, cutoff) == std::vector<std::string>{ "older" }));
    CHECK((lf::to_prune(old, lf::Limits{ 0, 0, 1 }, cutoff) == std::vector<std::string>{ "older" }));
    CHECK(lf::to_prune({}, lf::Limits{ 1, 1, 1 }, cutoff).empty());
}

static void test_save()
{
    const TempDir root("logs");
    g_root = root.path();
    const std::string dir = g_root + "/logs/uploads";
    const std::string a = lf::save(dir, lf::UPLOAD, "a", NOW);
    const std::string b = lf::save(dir, lf::UPLOAD, "b", NOW);   // the same second
    CHECK(a == dir + "/upload_20261011_120000.url" && b == dir + "/upload_20261011_120000_1.url");
    CHECK(read("logs/uploads/upload_20261011_120000_1.url") == "b");
    const auto names = lf::list(dir, lf::UPLOAD);
    CHECK(names.size() == 2 && names[0] == "upload_20261011_120000_1.url");
    // An upload is an Internet shortcut: a .txt beside it is not one of them,
    // and a report never a .url.
    write("logs/uploads/upload_20261011_130000.txt", "x");
    CHECK(lf::list(dir, lf::UPLOAD).size() == 2);
    CHECK(lf::parse("upload_20261011_130000.url", lf::UPLOAD) && !lf::parse("upload_20261011_130000.txt", lf::UPLOAD));
    CHECK(!lf::parse("report_20261011_130000.url", lf::REPORT));
    CHECK(lf::shortcut("https://bpa.st/AAAA", "https://bpa.st/remove/BBBB", "2026-10-11 14:25") ==
          "[InternetShortcut]\r\nURL=https://bpa.st/AAAA\r\n\r\n[PlayShelf]\r\n"
          "Sent=2026-10-11 14:25\r\nRemoval=https://bpa.st/remove/BBBB\r\n");
    CHECK(lf::shortcut("https://gist.github.com/x/1", "", "") ==
          "[InternetShortcut]\r\nURL=https://gist.github.com/x/1\r\n\r\n[PlayShelf]\r\n");
}

static void test_prune()
{
    const TempDir root("logs");
    g_root = root.path();
    const std::string dir = g_root + "/logs/reports";
    write_size("logs/reports/report_20261011_100000.txt", 10);
    write_size("logs/reports/report_20261010_100000.txt", 10);
    write_size("logs/reports/report_20261009_100000.txt", 10);
    write_size("logs/reports/report_20260101_100000.txt", 10);
    write_size("logs/reports/notes.txt", 10);                      // not a report: never removed
    write_size("logs/reports/crash_20200101_000000.txt", 10);      // nor another prefix

    CHECK(lf::prune(dir, lf::REPORT, lf::Limits{}, NOW) == 0);
    CHECK(lf::prune(dir, lf::REPORT, lf::Limits{ 0, 30, 0 }, NOW) == 1);
    CHECK(!there("logs/reports/report_20260101_100000.txt") && there("logs/reports/report_20261009_100000.txt"));
    CHECK(lf::prune(dir, lf::REPORT, lf::Limits{ 2, 0, 0 }, NOW) == 1);
    CHECK(!there("logs/reports/report_20261009_100000.txt"));
    CHECK(there("logs/reports/report_20261011_100000.txt") && there("logs/reports/report_20261010_100000.txt"));
    CHECK(there("logs/reports/notes.txt") && there("logs/reports/crash_20200101_000000.txt"));
    CHECK(lf::prune(g_root + "/missing", lf::REPORT, lf::Limits{ 1, 1, 1 }, NOW) == 0);
}

int main()
{
    setenv("TZ", "UTC", 1);
    tzset();
    test_names();
    test_list();
    test_to_prune();
    test_save();
    test_prune();
    return CHECK_DONE("log_files names, order and limits tests passed");
}
