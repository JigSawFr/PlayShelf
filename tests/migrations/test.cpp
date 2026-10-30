// Host tests for source/util/migrations.cpp: a first start runs every step,
// in order, and records the last and the build in migrations.json; the next
// start runs none; a start after an update runs only the new ones; the steps
// of the start and of the system (two calls) add up; a file unreadable runs
// them all again; an earlier build lowers a later one's level to its own,
// and the later one runs the steps above it again; going back to an earlier
// build sets the level to 0; which installs count as earlier.
#include "check.h"
#include "fs.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <string>
#include <vector>

#include <aurore/extern/nlohmann/json.hpp>

#include "app_id.h"
#include "util/migrations.hpp"

namespace mg = migrations;

// The steps of a made-up app: three of them.
enum : int
{
    DataDir  = 1,
    LogsDirs = 2,
    NandDirs = 3,
};

namespace
{
std::vector<int> g_ran;
std::vector<std::string> g_lines;   // the log, "W " before a warning

void log_line(bool warning, const std::string& line)
{
    g_lines.push_back((warning ? "W " : "") + line);
}

// Whether a line of the log holds `part`.
bool logged(const std::string& part)
{
    for (const std::string& l : g_lines)
        if (l.find(part) != std::string::npos) return true;
    return false;
}

mg::Step step(int level, size_t moved = 0)
{
    return { level, "test", [level, moved]() {
                g_ran.push_back(level);
                return moved;
            } };
}

std::vector<mg::Step> start_steps()
{
    return { step(DataDir, 2), step(LogsDirs) };
}

// Both calls of a start (main.cpp: before and after the system's folder).
size_t start(const std::string& file, const std::string& version)
{
    return mg::run(file, start_steps(), version) + mg::run(file, { step(NandDirs) }, version);
}

mg::State state(const std::string& file)
{
    mg::State s;
    mg::read(file, &s);
    return s;
}
}   // namespace

static void test_first_and_next()
{
    TempDir tmp("migrations");
    const std::string file = tmp.path() + "/data/migrations.json";   // its folder not there yet
    mg::State s;
    CHECK(!mg::read(file, &s) && s.level == mg::None && s.version.empty());
    g_ran.clear();
    g_lines.clear();
    CHECK(mg::run(file, start_steps(), "1.2.0", log_line) == 2);
    CHECK((g_ran == std::vector<int>{ DataDir, LogsDirs }));
    CHECK(g_lines.size() == 4);
    CHECK(logged("migrations: no " + file + " (a first start, or a build from before it): steps 1 to 2 to run"));
    CHECK(logged("migration 1 (test): 2 entries moved in ") && logged("migration 2 (test): 0 entries moved in "));
    CHECK(logged("migrations: level 2 and " APP_NAME " 1.2.0 written to " + file));
    const nlohmann::json j = nlohmann::json::parse(fs::read(file));
    CHECK(j["schema"] == mg::SCHEMA && j["level"] == LogsDirs && j["version"] == "1.2.0");

    // Then the system's step, in a call of its own.
    CHECK(mg::run(file, { step(NandDirs) }, "1.2.0") == 1);
    CHECK(state(file).level == NandDirs);

    // The next start: nothing, and nothing written.
    g_ran.clear();
    CHECK(mg::write(file, { mg::latest(), "1.2.0" }));
    const std::string before = fs::read(file);
    g_lines.clear();
    CHECK(mg::run(file, start_steps(), "1.2.0", log_line) == 0);
    CHECK(g_ran.empty() && fs::read(file) == before);
    CHECK(g_lines.size() == 2 && logged("migrations: level 3 recorded, last started by " APP_NAME " 1.2.0") &&
          logged("migrations: steps 1 to 2 already done, nothing run"));

    // An update: nothing to run, the build recorded.
    g_lines.clear();
    CHECK(mg::run(file, { step(NandDirs) }, "1.3.0", log_line) == 0 && state(file).version == "1.3.0");
    CHECK(logged("migrations: " APP_NAME " 1.2.0 started last, an earlier build (an update since); this is " APP_NAME " 1.3.0"));
    CHECK(logged("migrations: step 3 already done") && logged("written to"));
}

static void test_update()
{
    TempDir tmp("migrations");
    const std::string file = tmp.path() + "/migrations.json";
    fs::write(file, "{\"schema\": 1, \"level\": 1, \"version\": \"1.1.0\"}");
    g_ran.clear();
    CHECK(start(file, "1.2.0") == 2);
    CHECK((g_ran == std::vector<int>{ LogsDirs, NandDirs }));
    CHECK(state(file).level == mg::latest() && state(file).version == "1.2.0");
}

static void test_unreadable()
{
    TempDir tmp("migrations");
    const std::string file = tmp.path() + "/migrations.json";
    for (const char* bad : { "", "x", "3", "[]", "{}", "{\"level\": -1}", "{\"level\": \"3\"}", "{\"level\": 1.5}" }) {
        fs::write(file, bad);
        mg::State s;
        CHECK(!mg::read(file, &s) && s.level == mg::None);
    }
    g_ran.clear();
    g_lines.clear();
    CHECK(mg::run(file, start_steps(), "1.2.0", log_line) == 2);
    CHECK(logged("W migrations: " + file + " unreadable (damaged): steps 1 to 2 to run again"));
    CHECK(start(file, "1.2.0") == 1 && state(file).level == mg::latest());

    // Not written (a folder in its place): a warning, the steps again next time.
    TempDir dir("migrations");
    const std::string blocked = dir.path() + "/migrations.json";
    fs::write(blocked + "/x", "");
    g_lines.clear();
    CHECK(mg::run(blocked, start_steps(), "1.2.0", log_line) == 2);
    CHECK(logged("W migrations: could not write " + blocked));

    // No version: still read.
    fs::write(file, "{\"level\": 2}");
    CHECK(state(file).level == 2 && state(file).version.empty());

    // An interrupted write: its .tmp is read in its place.
    std::remove(file.c_str());
    fs::write(file + ".tmp", "{\"level\": 3, \"version\": \"1.2.0\"}");
    CHECK(state(file).level == NandDirs);
}

static void test_later_then_earlier()
{
    TempDir tmp("migrations");
    const std::string file = tmp.path() + "/migrations.json";
    // A later build, with a step 5 this one does not know, then this one.
    CHECK(mg::write(file, { 5, "1.4.0" }));
    g_ran.clear();
    g_lines.clear();
    CHECK(mg::run(file, start_steps(), "1.2.0", log_line) == 0);
    CHECK(mg::run(file, { step(NandDirs) }, "1.2.0") == 0);
    CHECK(g_ran.empty());
    CHECK(logged("migrations: " APP_NAME " 1.4.0 started last, a later build; this is " APP_NAME " 1.2.0"));
    CHECK(logged("migrations: level 5 is a later build's (this one knows up to 3): back to 3"));
    // Its own level and version: the later build runs 4 and 5 again.
    CHECK(state(file).level == mg::latest() && state(file).version == "1.2.0");
}

static void test_earlier_build_next()
{
    TempDir tmp("migrations");
    const std::string file = tmp.path() + "/migrations.json";
    // No file: nothing written (every step runs anyway).
    g_lines.clear();
    CHECK(mg::earlier_build_next(file, log_line) && !fs::exists(file));
    CHECK(logged("every step runs at the next start anyway"));

    CHECK(mg::write(file, { mg::latest(), "1.2.0" }));
    g_lines.clear();
    CHECK(mg::earlier_build_next(file, log_line));
    CHECK(logged("is next: level 3 set to 0") && logged("level 0 and " APP_NAME " 1.2.0 written"));
    g_lines.clear();
    CHECK(mg::earlier_build_next(file, log_line) && logged("level already 0"));
    CHECK(state(file).level == mg::None && state(file).version == "1.2.0");
    // The earlier build (one from before this file) leaves it so; this one
    // is back: every step again, once.
    g_ran.clear();
    CHECK(start(file, "1.2.0") == 3 && start(file, "1.2.0") == 0);

    // Damaged: written as level 0.
    fs::write(file, "garbage");
    CHECK(mg::earlier_build_next(file) && state(file).level == mg::None);
}

static void test_is_earlier()
{
    CHECK(!mg::is_earlier("1.3.0", "1.2.0"));
    CHECK(mg::is_earlier("1.1.0", "1.2.0"));
    CHECK(mg::is_earlier("1.2.0", "1.2.0"));   // the same release, maybe another commit
    CHECK(mg::is_earlier("1.2.0-rc1", "1.2.0"));
    CHECK(mg::is_earlier("", "1.2.0"));         // a branch, a pull request, a commit
    CHECK(mg::is_earlier("garbage", "1.2.0"));
}

static void test_all()
{
    TempDir tmp("migrations");
    const std::string file = tmp.path() + "/migrations.json";
    CHECK(mg::write(file, { mg::latest(), "1.2.0" }));
    g_ran.clear();
    g_lines.clear();
    CHECK(mg::run(file, start_steps(), "1.2.0", log_line, true) == 2);
    CHECK((g_ran == std::vector<int>{ DataDir, LogsDirs }));
    CHECK(logged("migrations: steps 1, 2 run again, whatever level is recorded"));
    // The level recorded is not lowered.
    CHECK(state(file).level == mg::latest());

    // One or a few of them.
    g_ran.clear();
    g_lines.clear();
    CHECK(mg::run(file, { step(DataDir), step(NandDirs) }, "1.2.0", log_line, true) == 2);
    CHECK((g_ran == std::vector<int>{ DataDir, NandDirs }));
    CHECK(logged("migrations: steps 1, 3 run again") && state(file).level == mg::latest());

    // Over a file not there: the level the steps reached.
    std::remove(file.c_str());
    CHECK(mg::run(file, { step(LogsDirs) }, "1.2.0", nullptr, true) == 1 && state(file).level == LogsDirs);
}

int main()
{
    mg::set_latest(NandDirs);
    test_first_and_next();
    test_update();
    test_unreadable();
    test_later_then_earlier();
    test_earlier_build_next();
    test_is_earlier();
    test_all();
    return CHECK_DONE("migrations tests passed");
}
