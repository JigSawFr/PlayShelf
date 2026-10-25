// Host tests for source/util/config.cpp and the ".tmp" recovery of
// source/kit/util/paths.cpp: every field read on its own, out-of-range values
// put back to their defaults, round trip, recovery of a save that stopped
// between its remove and its rename, a damaged file put aside
// (config.json.bad) before anything saves over it.
#include "check.h"
#include "fs.hpp"
#include "temp_dir.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/config.hpp"
#include "util/paths.hpp"

// A save as the app makes one (ui::save_config_now): the content, then the
// atomic write; false when it refuses.
static bool save_config()
{
    config::Snapshot snap;
    return config::snapshot(snap) && config::write(snap);
}

static void write_config(const std::string& text)
{
    CHECK(paths::atomic_write(paths::config_file(), text));
}

static void test_defaults()
{
    std::remove(paths::config_file().c_str());
    config::load();
    const auto& c = config::get();
    CHECK(c.look.language == "system" && c.look.theme == "dark" && c.look.text_size == "auto" && c.look.effects == "auto");
    CHECK(!c.dev_mode && !c.frame_stats && c.look.glow && c.intro && c.support_reminder);
    CHECK(c.start_tab == "library" && c.logs_keep == 20 && c.logs_keep_days == 0 && c.logs_keep_mb == 0);
    CHECK(c.update_via.empty() && c.update_latest.empty() && !c.update_daily);   // the update method: asked the first time

    CHECK(config::loaded() == config::Loaded::Missing);   // a first run: nothing to tell
    for (const char* bad : { "", "not json", "[1, 2]", "42", "{\"language\": " }) {
        write_config(bad);
        config::get().dev_mode = true;
        config::load();
        CHECK(!config::get().dev_mode && config::get().look.language == "system");
        CHECK(config::loaded() == config::Loaded::PutAside);
    }
    std::remove(paths::config_file().c_str());
    std::remove((paths::config_file() + ".bad").c_str());
    std::remove((paths::config_file() + ".bad.1").c_str());
}

using fs::exists;

// A file that is there but cannot be read as settings is put aside before
// any save, the previous damaged copy kept; a missing one is a silent first run.
static void test_damaged()
{
    const std::string file = paths::config_file(), bad = file + ".bad", older = bad + ".1";
    const std::string first = R"({"dev_mode": true, "theme": "li)";
    std::string text;
    write_config(first);
    config::load();
    CHECK(config::loaded() == config::Loaded::PutAside && !config::get().dev_mode);
    CHECK(!exists(file) && paths::read_file(bad, text) && text == first);
    // The automatic saves at start-up then write a new file; the damaged one stays.
    CHECK(save_config() && exists(file));
    CHECK(paths::read_file(bad, text) && text == first);
    config::load();
    CHECK(config::loaded() == config::Loaded::Read);

    // Damaged again: the first .bad becomes .bad.1; a third drops the oldest.
    write_config("second");
    config::load();
    CHECK(config::loaded() == config::Loaded::PutAside);
    CHECK(paths::read_file(bad, text) && text == "second");
    CHECK(paths::read_file(older, text) && text == first);
    write_config("third");
    config::load();
    CHECK(paths::read_file(bad, text) && text == "third" && paths::read_file(older, text) && text == "second");

    // Only a damaged .tmp left (a save stopped before its rename): that one is put aside.
    CHECK(paths::atomic_write(file + ".tmp", "{ half"));
    std::remove(file.c_str());
    CHECK(exists(file + ".tmp"));
    config::load();
    CHECK(config::loaded() == config::Loaded::PutAside && !exists(file + ".tmp"));
    CHECK(paths::read_file(bad, text) && text == "{ half");

    // There but unreadable (a folder in its place) and it cannot be moved
    // (the .bad cannot be renamed either): saves refuse, nothing is replaced.
    CHECK(mkdir(file.c_str(), 0700) == 0);
    std::remove(older.c_str());
    CHECK(mkdir(older.c_str(), 0700) == 0);
    CHECK(paths::atomic_write(older + "/x", "keeps the folder non-empty"));
    config::load();
    CHECK(config::loaded() == config::Loaded::Stuck && exists(file));
    CHECK(!save_config());
    CHECK(paths::read_file(bad, text) && text == "{ half");
    std::remove((older + "/x").c_str());
    CHECK(rmdir(older.c_str()) == 0 && rmdir(file.c_str()) == 0);

    // Missing again: a first run, silent, and saves work.
    std::remove(bad.c_str());
    config::load();
    CHECK(config::loaded() == config::Loaded::Missing && save_config());
}

// Each field on its own: a value of the wrong type keeps that field's
// default, the others are still read; a value out of its range goes back to
// its default (sanitize).
static void test_fields()
{
    write_config(R"({"language": "fr", "theme": 3, "text_size": "large", "effects": "reduced", "dev_mode": "yes",
                    "frame_stats": true, "intro": false, "start_tab": "about", "logs_keep": 5, "logs_keep_days": 7,
                    "logs_keep_mb": "20", "update_daily": true, "update_via": "sphaira"})");
    config::load();
    const auto& c = config::get();
    CHECK(config::loaded() == config::Loaded::Read);
    CHECK(c.look.language == "fr" && c.look.theme == "dark" && c.look.text_size == "large" && c.look.effects == "reduced");
    CHECK(!c.dev_mode && c.frame_stats && !c.intro && c.start_tab == "about");
    CHECK(c.logs_keep == 5 && c.logs_keep_days == 7 && c.logs_keep_mb == 0);
    CHECK(c.update_daily && c.update_via == "sphaira");

    // Out of range: back to the default, one field at a time.
    write_config(R"({"language": "xx", "theme": "blue", "text_size": "huge", "effects": "wild",
                    "start_tab": "dashboard", "logs_keep": 7, "logs_keep_days": 2, "logs_keep_mb": 3,
                    "update_via": "ftp", "dev_builds_order": "random", "update_checked": "yesterday",
                    "update_snooze_version": "1.2.0", "update_snooze_until": "soon",
                    "support_reminded": "2026-13", "seen_version": "1234567890123456789012345678901234567890"})");
    config::load();
    CHECK(c.look.language == "system" && c.look.theme == "dark" && c.look.text_size == "auto" && c.look.effects == "auto");
    CHECK(c.start_tab == "library" && c.logs_keep == 20 && c.logs_keep_days == 0 && c.logs_keep_mb == 0);
    CHECK(c.update_via.empty() && c.dev_builds_order == "newest" && c.update_checked.empty());
    CHECK(c.update_snooze_version.empty() && c.update_snooze_until.empty());
    CHECK(c.support_reminded.empty() && c.seen_version.empty());

    // An integer out of int's reach is refused, not narrowed.
    write_config(R"({"logs_keep": 4294967301, "logs_keep_days": -4294967266})");
    config::load();
    CHECK(c.logs_keep == 20 && c.logs_keep_days == 0);
}

// What is saved reads back the same.
static void test_round_trip()
{
    config::get() = config::Config{};
    auto& c = config::get();
    c.look.language = "ja";
    c.look.theme = "light";
    c.look.text_size = "normal";
    c.look.effects = "full";
    c.dev_mode = true;
    c.frame_stats = true;
    c.look.glow = false;
    c.update_via = "inplace";
    c.update_daily = true;
    c.update_checked = "2026-10-24";
    c.update_latest = "1.2.0";
    c.update_snooze_version = "1.2.0";
    c.update_snooze_until = "2026-11-01";
    c.start_tab = "preferences";
    c.logs_keep = 50;
    c.logs_keep_days = 30;
    c.logs_keep_mb = 5;
    c.intro = false;
    c.dev_builds_order = "number";
    c.support_reminder = false;
    c.support_reminded = "2026-10-01";
    c.seen_version = "1.1.0";
    c.crash_seen = "crash_20261024_101010.txt";
    const config::Config before = c;
    CHECK(save_config());
    c = config::Config{};
    config::load();
    CHECK(c.look.language == before.look.language && c.look.theme == before.look.theme && c.look.text_size == before.look.text_size);
    CHECK(c.look.effects == before.look.effects && c.dev_mode && c.frame_stats && !c.look.glow);
    CHECK(c.update_via == "inplace" && c.update_daily && c.update_checked == "2026-10-24");
    CHECK(c.update_latest == "1.2.0" && c.update_snooze_version == "1.2.0" && c.update_snooze_until == "2026-11-01");
    CHECK(c.start_tab == "preferences" && c.logs_keep == 50 && c.logs_keep_days == 30 && c.logs_keep_mb == 5);
    CHECK(!c.intro && c.dev_builds_order == "number" && !c.support_reminder && c.support_reminded == "2026-10-01");
    CHECK(c.seen_version == "1.1.0" && c.crash_seen == "crash_20261024_101010.txt");
    std::string text;
    CHECK(paths::read_file(paths::config_file(), text) && text.find("\"schema\": 1") != std::string::npos);
}

static void test_tmp_recovery()
{
    // Stopped after removing config.json, before renaming config.json.tmp.
    config::get() = config::Config{};
    config::get().look.theme = "dark";
    CHECK(save_config());
    const std::string file = paths::config_file();
    CHECK(std::rename(file.c_str(), (file + ".tmp").c_str()) == 0);
    config::get() = config::Config{};
    config::load();
    CHECK(config::get().look.theme == "dark");

    // The next save must not truncate that .tmp, the only good copy: it is
    // promoted first, and a save that fails afterwards still leaves it.
    std::string text;
    CHECK(paths::atomic_write(file, R"({"theme": "light"})"));
    CHECK(paths::read_file(file, text) && text == R"({"theme": "light"})");
    CHECK(std::rename(file.c_str(), (file + ".tmp").c_str()) == 0);
    CHECK(paths::atomic_write(file, R"({"theme": "dark"})"));
    CHECK(paths::read_file(file, text) && text == R"({"theme": "dark"})");
    struct stat st;
    CHECK(stat((file + ".tmp").c_str(), &st) != 0);
    // Same, but the write fails (file size limit): the old content survives.
    CHECK(std::rename(file.c_str(), (file + ".tmp").c_str()) == 0);
    std::signal(SIGXFSZ, SIG_IGN);
    rlimit saved;
    CHECK(getrlimit(RLIMIT_FSIZE, &saved) == 0);
    rlimit small = saved;
    small.rlim_cur = 64;
    CHECK(setrlimit(RLIMIT_FSIZE, &small) == 0);
    const bool wrote = paths::atomic_write(file, std::string(4096, ' '));
    CHECK(setrlimit(RLIMIT_FSIZE, &saved) == 0);
    CHECK(!wrote);
    CHECK(paths::read_file(file, text) && text == R"({"theme": "dark"})");

    // A leftover .tmp never wins over the file itself.
    write_config(R"({"theme": "light"})");
    CHECK(paths::atomic_write(file + ".tmp", R"({"theme": "dark"})"));
    config::load();
    CHECK(config::get().look.theme == "light");

    // Both missing: nothing to read.
    std::remove(file.c_str());
    std::remove((file + ".tmp").c_str());
    CHECK(!paths::read_file(file, text));

    // A first write cut short (no file before it) leaves a ".part", never a
    // ".tmp": nothing is read in its place, it is not taken for a damaged
    // file, and the next write drops it.
    fs::write(file + ".part", "{\"theme\": \"da");
    CHECK(!paths::read_file(file, text) && !paths::exists_or_pending(file) && !paths::file_exists(file));
    config::get() = config::Config{};
    config::load();
    CHECK(config::loaded() == config::Loaded::Missing && config::get().look.theme == "dark");
    CHECK(paths::atomic_write(file, R"({"theme": "dark"})"));
    CHECK(paths::read_file(file, text) && text == R"({"theme": "dark"})");
    CHECK(!exists(file + ".part") && !exists(file + ".tmp"));
    // The same write failing (file size limit) leaves no ".tmp" either.
    std::remove(file.c_str());
    CHECK(setrlimit(RLIMIT_FSIZE, &small) == 0);
    const bool first = paths::atomic_write(file, std::string(4096, ' '));
    CHECK(setrlimit(RLIMIT_FSIZE, &saved) == 0);
    CHECK(!first && !exists(file) && !exists(file + ".tmp") && !exists(file + ".part"));
    CHECK(!paths::exists_or_pending(file));
    // A file and an older ".tmp" next to it (a write stopped before its last
    // step): the next write replaces both with its own content.
    write_config(R"({"theme": "light"})");
    fs::write(file + ".tmp", "{\"theme\": \"dark\"}");
    CHECK(paths::atomic_write(file, R"({"theme": "system"})"));
    CHECK(paths::read_file(file, text) && text == R"({"theme": "system"})" && !exists(file + ".tmp"));
}

int main()
{
    TempDir tmp("config", TempDir::Enter);   // paths::data_dir() is ./playshelf_data on the host

    test_defaults();
    test_damaged();
    test_fields();
    test_round_trip();
    test_tmp_recovery();

    CHECK(tmp.remove());
    return CHECK_DONE("config fields, ranges, round trip, .tmp recovery and damaged-file assertions passed");
}
