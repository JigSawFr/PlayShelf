// migrations — the moves of the data folder's earlier layouts, run once
// each instead of at every start: data_dir()/migrations.json keeps the last one done and the
// build that last started, and a start runs only the steps above it, in
// order. On the SD card each look for a file costs about a millisecond, and
// the old names are missing at nearly every start. Plain C++ (no Aurore):
// host-tested (tests/migrations).
//
//   {"schema": 1, "level": 3, "version": "1.2.0"}
//
// A new layout is a new step with the next number; a step whose shim goes
// leaves its number unused, never given to another step. A file missing or unreadable reads as
// level 0: every step again, which is harmless (each one moves only what it
// finds).
//
// An earlier build may write the data in its own, earlier layout; the steps
// above it run again at the next start of a later one:
// - one that reads this file and finds a level above its own latest() (a
//   later build's) lowers it to its own, and runs nothing;
// - one from before this file cannot: going back to an earlier build from
//   the app (the previous build, an install that is not a newer release)
//   calls earlier_build_next() first, which sets the level to 0. A .nro
//   copied over by hand is not seen.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace migrations
{

// The steps are the app's: each has a number that never changes, 1 for the
// first. None is the level before any; set_latest() tells the highest this
// build knows (0 until called: no step yet).
enum Level : int
{
    None = 0,
};
void set_latest(int level);
int  latest();

constexpr int SCHEMA = 1;   // "schema" in migrations.json

struct State
{
    int         level = None;   // the last step done
    std::string version;        // the build that last started ("1.2.0"); "" when unknown
};

// migrations.json; false (and `out` left as State{}) when it is missing or
// unreadable.
bool read(const std::string& file, State* out);
bool write(const std::string& file, const State& state);

// A line for the log (English), and whether it is a warning (else
// information). Every call below that takes one logs what it found and did.
using Log = std::function<void(bool warning, const std::string& line)>;

struct Step
{
    int                     level;
    const char*             name;   // for the log
    std::function<size_t()> run;    // how many entries it moved
};

// Each step of `steps` (in ascending order) above the level `file` records,
// after a level above latest() (a later build's) is lowered to it; then
// the level reached and `version` (this build's) written to `file`, when
// either changed. Logged, when `log` is given: what the file holds (or that
// it is missing or unreadable), another build seen since (earlier or later),
// each step run (entries moved, time taken) or that they were all done, what
// was written or why it could not be. Returns how many steps ran. A step
// runs again at the next start when the file could not be written. `all`:
// every step of `steps`, whatever level is recorded, which it never lowers
// (the developer tools' "Run the migrations again", one or a few of them).
size_t run(const std::string& file, const std::vector<Step>& steps, const std::string& version,
           const Log& log = nullptr, bool all = false);

// An earlier build (or one whose layout is not known) is about to replace
// this one: the level set to 0, so that this build, or a later one, runs
// every step again at its next start. Nothing when there is no file (they
// run anyway). Logged, when `log` is given. False when the file could not be
// written.
bool earlier_build_next(const std::string& file, const Log& log = nullptr);

// Whether installing `version` (a release's, "1.2.0"; "" for a build of a
// branch, a pull request or a commit) replaces `running` with an earlier
// build or one whose layout is not known: anything but a newer release.
bool is_earlier(const std::string& version, const std::string& running);

}   // namespace migrations
