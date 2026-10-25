// Host tests for source/util/rollback.cpp and paths::replace_file: when the
// previous build is offered at start-up and when the PIN lets it go back, its label and the start check on
// disk (damaged files included), the swap of <nro> and <nro>.previous (back
// and forth, with no previous build, with no current one), and an install
// or a swap stopped between its renames, finished or undone.
#include "check.h"
#include "fs.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <string>
#include <unistd.h>

#include "util/paths.hpp"
#include "util/rollback.hpp"

// The suite's folder (main()).
static std::string g_dir;

static void test_offer()
{
    // Nothing without a previous build, whatever happened.
    CHECK(!rollback::offer_at_start(true, 3, false));
    // The build installed started once without reaching its first screen:
    // offered (before PlayShelf's own lock screen: it may be what crashes).
    CHECK(rollback::offer_at_start(true, 1, true));
    CHECK(rollback::offer_at_start(true, 3, true));
    // Its first start (nothing failed yet), or no check at all: nothing.
    CHECK(!rollback::offer_at_start(true, 0, true));
    CHECK(!rollback::offer_at_start(false, 0, true));
    CHECK(!rollback::offer_at_start(false, 2, true));   // a check that is not there counts nothing
}

static void test_pin()
{
    // Going back asks the system's PIN screen: only the PIN, or no PIN set,
    // lets it through. Left with B, or the screen failing: this build starts.
    CHECK(rollback::go_back_allowed(true, false));
    CHECK(rollback::go_back_allowed(false, true));
    CHECK(!rollback::go_back_allowed(false, false));
}

static void test_files()
{
    const std::string label = g_dir + "/previous_build.json", check = g_dir + "/boot_check.json";
    std::string l;
    CHECK(!rollback::read_label(label, &l));
    CHECK(rollback::write_label(label, "1.2.0 (abc1234)") && rollback::read_label(label, &l) && l == "1.2.0 (abc1234)");
    fs::write(label, "{\"label\": 3}");
    CHECK(!rollback::read_label(label, &l));
    fs::write(label, "not json");
    CHECK(!rollback::read_label(label, &l));

    rollback::Check c;
    CHECK(!rollback::read_check(check, &c));
    c.label = "PR #38";
    c.starts = 1;
    CHECK(rollback::write_check(check, c));
    rollback::Check back;
    CHECK(rollback::read_check(check, &back) && back.label == "PR #38" && back.starts == 1);
    fs::write(check, "{\"starts\": -4}");
    CHECK(rollback::read_check(check, &back) && back.starts == 0 && back.label.empty());
    fs::write(check, "[]");
    CHECK(!rollback::read_check(check, &back));
}

static void test_swap()
{
    const std::string nro = g_dir + "/playshelf.nro", prev = rollback::previous_file(nro);
    CHECK(prev == nro + ".previous");
    std::string err;
    fs::write(nro, "new");
    CHECK(!rollback::swap(nro, &err) && !err.empty() && fs::content_of(nro) == "new");   // no previous build
    fs::write(prev, "old");
    CHECK(rollback::swap(nro, &err) && fs::content_of(nro) == "old" && fs::content_of(prev) == "new");
    CHECK(rollback::swap(nro, &err) && fs::content_of(nro) == "new" && fs::content_of(prev) == "old");   // and back
    CHECK(fs::content_of(nro + ".old") == "<none>");
    // No current build (deleted by hand): the previous one takes its place.
    std::remove(nro.c_str());
    CHECK(rollback::swap(nro, &err) && fs::content_of(nro) == "old" && fs::content_of(prev) == "<none>");
}

// A power cut between the renames of paths::replace_file (an install, a
// swap): what start-up (rollback::recover) and the next one find.
static void test_recover()
{
    const std::string nro = g_dir + "/cut.nro", prev = rollback::previous_file(nro), old = nro + ".old";
    const std::string fresh = nro + ".new";
    std::string err;
    std::remove(nro.c_str());
    std::remove(prev.c_str());
    std::remove(old.c_str());

    // Stopped after the first step: the build that ran only aside, the new
    // one still beside it. Put back; the install simply did not happen.
    fs::write(old, "running");
    fs::write(fresh, "new");
    fs::write(prev, "older");
    rollback::recover(nro);
    CHECK(fs::content_of(nro) == "running" && fs::content_of(old) == "<none>" && fs::content_of(prev) == "older");
    // And again: nothing left to do.
    rollback::recover(nro);
    CHECK(fs::content_of(nro) == "running" && fs::content_of(prev) == "older");

    // Stopped after the second: the new one in place, the build that ran
    // aside. It becomes the previous one, as the install meant.
    std::remove(fresh.c_str());
    fs::write(nro, "new");
    fs::write(old, "running");
    rollback::recover(nro);
    CHECK(fs::content_of(nro) == "new" && fs::content_of(prev) == "running" && fs::content_of(old) == "<none>");

    // A swap stopped after its second step: the previous build in place, the
    // one that ran aside, no previous left. The next swap finishes the first,
    // then swaps back.
    fs::write(nro, "older");
    fs::write(old, "new");
    std::remove(prev.c_str());
    CHECK(rollback::swap(nro, &err) && fs::content_of(nro) == "new" && fs::content_of(prev) == "older" && fs::content_of(old) == "<none>");

    // An install that finds a cut one: the earlier one is finished first,
    // then this one replaces the build in place.
    fs::write(nro, "new");
    fs::write(old, "running");
    fs::write(fresh, "newest");
    CHECK(paths::replace_file(nro, fresh, prev, &err));
    CHECK(fs::content_of(nro) == "newest" && fs::content_of(prev) == "new" && fs::content_of(old) == "<none>" && fs::content_of(fresh) == "<none>");
    // Without a name to keep it as, what was aside goes.
    fs::write(old, "stale");
    fs::write(fresh, "again");
    CHECK(paths::replace_file(nro, fresh, "", &err));
    CHECK(fs::content_of(nro) == "again" && fs::content_of(old) == "<none>" && fs::content_of(prev) == "new");
    // A missing new file: the build in place stays, nothing left aside.
    CHECK(!paths::replace_file(nro, fresh, prev, &err) && !err.empty());
    CHECK(fs::content_of(nro) == "again" && fs::content_of(old) == "<none>" && fs::content_of(prev) == "new");
}

int main()
{
    const TempDir tmp("rollback");
    g_dir = tmp.path();

    test_offer();
    test_pin();
    test_files();
    test_swap();
    test_recover();
    return CHECK_DONE("rollback offer, label and start check files, swap and recovery assertions passed");
}
