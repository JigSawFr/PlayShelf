// Host tests for source/util/data_cleanup.cpp: what "Clear the cache" and
// "Delete the log files" remove (every file of cache/ and its subfolders;
// every file of logs/ and its subfolders but uploads/ and the files the app
// keeps there, keep_log()), that folders and the rest of the data folder stay, the removal's
// count (a file gone since the plan is neither removed nor failed, one that
// cannot be removed is failed), and the size shown.
#include "check.h"
#include "fs.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "util/data_cleanup.hpp"

namespace dc = data_cleanup;

namespace
{
std::string g_root;

void write(const std::string& rel, size_t size)
{
    fs::write(g_root + "/" + rel, std::string(size, 'x'));
}

void dir(const std::string& rel)
{
    REQUIRE(mkdir((g_root + "/" + rel).c_str(), 0755) == 0);
}

bool there(const std::string& rel)
{
    return fs::exists(g_root + "/" + rel);
}

// A data folder as an app leaves it, in `root`.
void make_tree(const TempDir& root)
{
    g_root = root.path();
    write("config.json", 300);
    write("history.json", 400);
    write("watch.json", 50);
    write("github_token", 40);
    dir("backups");
    write("backups/settings_20261010.json", 900);
    dir("profiles");
    write("profiles/school.json", 120);
    dir("exports");
    write("exports/activity.csv", 2000);
    dir("cache");
    write("cache/activity_all.bin", 5000);
    write("cache/activity_0011223344556677.bin", 3000);
    write("cache/dev_builds.json", 700);
    dir("cache/icons");
    write("cache/icons/lang", 1);
    write("cache/icons/0100000000010000.jpg", 6000);
    write("cache/icons/0100000000020000.jpg", 7000);
    dir("nand");
    dir("nand/sysmmc");
    dir("nand/sysmmc/cache");
    write("nand/sysmmc/cache/activity_all.bin", 4000);
    write("nand/sysmmc/history.json", 100);
    dir("logs");
    dir("logs/crashes");
    write("logs/crashes/crash_20261009_141000.txt", 200);
    dir("logs/reports");
    write("logs/reports/report_20261009_141203.txt", 3000);
    write("logs/reports/report_20261009_141203_1.txt", 3100);
    dir("logs/play_timer");
    write("logs/play_timer/log.csv", 9000);
    write("logs/play_timer/log.old.csv", 8000);
    write("logs/play_timer/block.json", 1000);
    write("logs/play_timer/block.json.tmp", 1100);
    dir("logs/uploads");
    write("logs/uploads/upload_20261009_150000.url", 150);
    write("logs/uploads/upload_20261009_150000_1.url.tmp", 160);
}
}   // namespace

static void test_cache_plan()
{
    const TempDir root("cleanup");
    make_tree(root);
    const dc::Plan p = dc::cache(g_root);
    CHECK(p.files.size() == 7);
    CHECK(p.bytes == 5000 + 3000 + 700 + 1 + 6000 + 7000 + 4000);
    // Sorted, full paths, the icons' folder walked, each system's cache too.
    if (CHECK(p.files.size() == 7)) {
        CHECK(p.files[0] == g_root + "/cache/activity_0011223344556677.bin");
        CHECK(p.files[1] == g_root + "/cache/activity_all.bin");
        CHECK(p.files[2] == g_root + "/cache/dev_builds.json");
        CHECK(p.files[3] == g_root + "/cache/icons/0100000000010000.jpg");
        CHECK(p.files[5] == g_root + "/cache/icons/lang");
        CHECK(p.files[6] == g_root + "/nand/sysmmc/cache/activity_all.bin");
    }
    const dc::Outcome out = dc::remove(p);
    CHECK(out.removed == 7);
    CHECK(out.bytes == p.bytes);
    CHECK(out.failed == 0);
    // Only files went: the folders stay for the icon store, the rest is untouched.
    CHECK(there("cache") && there("cache/icons") && there("nand/sysmmc/cache") && there("nand/sysmmc/history.json"));
    CHECK(!there("cache/activity_all.bin") && !there("cache/icons/lang"));
    CHECK(there("config.json") && there("history.json") && there("watch.json") && there("github_token"));
    CHECK(there("backups/settings_20261010.json") && there("profiles/school.json") && there("exports/activity.csv"));
    CHECK(there("logs/crashes/crash_20261009_141000.txt"));
    // Nothing left: an empty plan.
    const dc::Plan again = dc::cache(g_root);
    CHECK(again.files.empty() && again.bytes == 0);
}

static void test_logs_plan()
{
    const TempDir root("cleanup");
    make_tree(root);
    dir("logs/old");   // kept by their path from logs/: elsewhere, the same names go
    dir("logs/old/uploads");
    write("logs/old/uploads/upload_20261009_150000.url", 10);
    write("logs/block.json", 20);
    // Until the app keeps them, its files under logs/ go with the rest.
    CHECK(dc::logs(g_root).files.size() == 9);
    dc::keep_log("play_timer/block.json");
    dc::keep_log("play_timer/block.json.tmp");
    dc::keep_log("play_timer/block.json");   // once is enough
    const dc::Plan p = dc::logs(g_root);
    CHECK(p.files.size() == 7);
    CHECK(p.bytes == 200 + 3000 + 3100 + 9000 + 8000 + 10 + 20);
    const dc::Outcome out = dc::remove(p);
    CHECK(out.removed == 7 && out.failed == 0);
    CHECK(!there("logs/crashes/crash_20261009_141000.txt") && !there("logs/reports/report_20261009_141203.txt") &&
          !there("logs/play_timer/log.csv") && !there("logs/play_timer/log.old.csv"));
    CHECK(!there("logs/old/uploads/upload_20261009_150000.url") && !there("logs/block.json"));
    // The folders stay.
    CHECK(there("logs/crashes") && there("logs/reports") && there("logs/play_timer"));
    // Not logs: the removal links of reports sent online, the files the app keeps.
    CHECK(there("logs/uploads/upload_20261009_150000.url") && there("logs/uploads/upload_20261009_150000_1.url.tmp") &&
          there("logs/play_timer/block.json") &&
          there("logs/play_timer/block.json.tmp"));
    CHECK(there("cache/activity_all.bin"));
}

// This run's session file is still being written: it stays, the others go.
static void test_logs_keep_session()
{
    const TempDir root("cleanup");
    make_tree(root);
    dir("logs/sessions");
    write("logs/sessions/session_20261009_141000.txt", 50);
    write("logs/sessions/session_20261010_090000.txt", 60);
    const dc::Plan all = dc::logs(g_root);
    const dc::Plan p = dc::logs(g_root, "session_20261010_090000.txt");
    CHECK(p.files.size() + 1 == all.files.size() && p.bytes + 60 == all.bytes);
    CHECK(dc::remove(p).failed == 0);
    CHECK(there("logs/sessions/session_20261010_090000.txt") && !there("logs/sessions/session_20261009_141000.txt"));
}

static void test_missing_folders()
{
    const TempDir root("cleanup");
    g_root = root.path();
    CHECK(dc::cache(g_root).files.empty());
    CHECK(dc::logs(g_root).files.empty());
    const dc::Outcome out = dc::remove(dc::Plan{});
    CHECK(out.removed == 0 && out.failed == 0 && out.bytes == 0);
}

static void test_remove_counts()
{
    const TempDir root("cleanup");
    make_tree(root);
    dc::Plan p = dc::cache(g_root);
    // Gone since the plan: neither removed nor failed.
    REQUIRE(std::remove((g_root + "/cache/dev_builds.json").c_str()) == 0);
    // Not removable: a non-empty folder where a file was listed.
    REQUIRE(std::remove((g_root + "/cache/activity_all.bin").c_str()) == 0);
    dir("cache/activity_all.bin");
    write("cache/activity_all.bin/x", 1);
    const dc::Outcome out = dc::remove(p);
    CHECK(out.removed == 5);
    CHECK(out.bytes == 3000 + 1 + 6000 + 7000 + 4000);
    CHECK(out.failed == 1);
}

static void test_size_unit()
{
    uint64_t v = 99;
    CHECK(dc::size_unit(0, &v) == dc::Unit::KB && v == 0);
    CHECK(dc::size_unit(1, &v) == dc::Unit::KB && v == 1);          // never "0 KB" for a file
    CHECK(dc::size_unit(1024, &v) == dc::Unit::KB && v == 1);
    CHECK(dc::size_unit(1025, &v) == dc::Unit::KB && v == 2);
    CHECK(dc::size_unit(1024 * 1024 - 1, &v) == dc::Unit::KB && v == 1024);
    CHECK(dc::size_unit(1024 * 1024, &v) == dc::Unit::MB && v == 1);
    CHECK(dc::size_unit(1024 * 1024 * 3 / 2, &v) == dc::Unit::MB && v == 2);   // 1.5 MB, rounded
    CHECK(dc::size_unit(16ull * 1024 * 1024 + 100, &v) == dc::Unit::MB && v == 16);
}

int main()
{
    test_cache_plan();
    test_logs_plan();
    test_logs_keep_session();
    test_missing_folders();
    test_remove_counts();
    test_size_unit();
    return CHECK_DONE("data_cleanup cache, logs, removal and size tests passed");
}
