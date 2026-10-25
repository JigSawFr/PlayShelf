// Host tests for source/util/session_log.cpp and source/util/crash_report.cpp:
// a run's log file (its header, the lines logged before it, one line per
// message written in batches, at once for a warning, before the end line and
// a crash's, the end line, its size limit), the lines kept in memory for the
// crash files (cut, oldest first, as many as fit), a run that never ended
// found at the next start, and the crash files (their first line, the
// system and session lines, the details, the last lines logged, a free name
// within a second, the session's end line pointing at the file).
#include "check.h"
#include "fs.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "util/crash_report.hpp"
#include "util/log_files.hpp"
#include "util/paths.hpp"
#include "util/session_log.hpp"

namespace sl = session_log;

namespace
{
// 2026-10-15 12:00:00 UTC (TZ is UTC in main()).
constexpr std::time_t NOW = 1792065600;

using fs::read;

void write(const std::string& path, const std::string& content)
{
    REQUIRE(paths::ensure_dir(path.substr(0, path.find_last_of('/'))));
    REQUIRE(paths::atomic_write(path, content));
}

size_t count(const std::string& text, const std::string& what)
{
    size_t n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size())) n++;
    return n;
}
}   // namespace

static void test_format()
{
    CHECK(sl::format(NOW + 5, 42, 'I', "start-up: first screen") == "12:00:05.042 I start-up: first screen\n");
    // Several lines: the next ones indented; a final line break or CR dropped.
    CHECK(sl::format(NOW, 7, 'E', "first\r\nsecond\n") == "12:00:00.007 E first\n    second\n");
    CHECK(sl::format(NOW, 1234, 'W', "") == "12:00:00.000 W \n");
}

static void test_session()
{
    sl::reset();
    const std::string dir = paths::sessions_dir();
    // Before the file: in memory only.
    sl::add(NOW, 1, 'I', "before the file");
    CHECK(sl::current_name().empty());
    CHECK(sl::recent() == "12:00:00.001 I before the file\n");

    const std::string name = sl::start(dir, NOW, "=== PlayShelf session ===\nbuild   : test\n\n");
    CHECK(name == "session_20261015_120000.txt");
    CHECK(sl::current_name() == name);
    // Started again: the same file.
    CHECK(sl::start(dir, NOW + 1, "other\n") == name);
    sl::add(NOW + 2, 500, 'W', "a warning");
    std::string text = read(dir + "/" + name);
    CHECK(text == "=== PlayShelf session ===\nbuild   : test\n\n12:00:00.001 I before the file\n"
                  "12:00:02.500 W a warning\n");
    CHECK(!sl::finished(text));
    CHECK(sl::end_reason(text).empty());

    sl::end("clean exit");
    sl::add(NOW + 3, 0, 'I', "after the end");   // memory only
    sl::end("twice");
    text = read(dir + "/" + name);
    CHECK(sl::finished(text));
    CHECK(sl::end_reason(text) == "clean exit");
    CHECK(text.find("after the end") == std::string::npos && text.find("twice") == std::string::npos);
    CHECK(sl::recent().find("after the end") != std::string::npos);

    // Another run in the same second: the next free name.
    sl::reset();
    CHECK(sl::start(dir, NOW, "h\n\n") == "session_20261015_120000_1.txt");
    sl::reset();
}

static void test_batches()
{
    sl::reset();
    const std::string dir = paths::logs_dir() + "/batches";
    const std::string name = sl::start(dir, NOW, "h\n\n");
    REQUIRE(!name.empty());
    const std::string path = dir + "/" + name;
    // An info line waits in memory, the file closed meanwhile ...
    sl::add(NOW, 1, 'I', "one");
    sl::add(NOW, 2, 'D', "two");
    CHECK(read(path) == "h\n\n");
    CHECK(!sl::due());
    CHECK(sl::recent().find("two") != std::string::npos);
    // ... and goes with the next lines once FLUSH_MS have passed.
    usleep((useconds_t)(sl::FLUSH_MS + 50) * 1000);
    CHECK(sl::due());
    sl::add(NOW, 3, 'I', "three");
    CHECK(read(path) == "h\n\n12:00:00.001 I one\n12:00:00.002 D two\n12:00:00.003 I three\n");
    CHECK(!sl::due());
    // flush() writes what waits; a warning or an error goes at once.
    sl::add(NOW, 4, 'I', "four");
    sl::flush();
    CHECK(read(path).find("I four\n") != std::string::npos);
    sl::flush();   // nothing waits: nothing written twice
    CHECK(count(read(path), "I four\n") == 1);
    sl::add(NOW, 5, 'I', "five");
    sl::add(NOW, 6, 'E', "six");
    CHECK(read(path).find("I five\n12:00:00.006 E six\n") != std::string::npos);
    // More than the buffer holds: written as it fills, nothing lost.
    const std::string line(1000, 'y');
    for (int i = 0; i < 40; i++) sl::add(NOW, 0, 'I', line);
    CHECK(count(read(path), line) >= 32);
    sl::add(NOW, 7, 'I', std::string(sl::PENDING_BYTES + 10, 'z'));   // longer than the buffer
    sl::add(NOW, 8, 'I', "last words");
    // The end line after the lines still waiting.
    sl::end("clean exit");
    std::string text = read(path);
    CHECK(count(text, line) == 40 && count(text, std::string(sl::PENDING_BYTES + 10, 'z')) == 1);
    CHECK(text.find("I last words\n=== end: clean exit ===\n") != std::string::npos);
    sl::reset();

    // A crash: the lines still waiting, then its end line.
    const std::string crashed = sl::start(dir, NOW + 60, "h\n\n");
    REQUIRE(!crashed.empty());
    sl::add(NOW, 9, 'I', "just before");
    sl::end_crash("crash_20261015_120100.txt");
    text = read(dir + "/" + crashed);
    CHECK(text == "h\n\n12:00:00.009 I just before\n=== end: crash, see logs/crashes/crash_20261015_120100.txt ===\n");
    sl::reset();
}

static void test_memory()
{
    sl::reset();
    // Over RING_LINES: the oldest go; each line cut on a whole character.
    for (size_t i = 0; i < sl::RING_LINES + 5; i++) sl::add(NOW, 0, 'I', "line " + std::to_string(i));
    std::string r = sl::recent();
    CHECK(count(r, "\n") == sl::RING_LINES);
    CHECK(r.find("I line 4\n") == std::string::npos && r.compare(0, 22, "12:00:00.000 I line 5\n") == 0);
    CHECK(r.compare(r.size() - std::strlen("I line 84\n"), std::string::npos, "I line 84\n") == 0);

    sl::reset();
    std::string longer(500, 'a');
    longer.replace(sl::LINE_BYTES - 16, 3, "\xC3\xA9\xC3");   // é across the cut, then half a character
    sl::add(NOW, 0, 'I', longer);
    r = sl::recent();
    CHECK(r.size() <= sl::LINE_BYTES - 1 && r.back() == '\n');
    // Never half of a UTF-8 sequence.
    CHECK((unsigned char)r[r.size() - 2] < 0x80 || ((unsigned char)r[r.size() - 2] & 0xC0) == 0x80);

    // A small buffer: the last lines that fit, whole.
    sl::reset();
    sl::add(NOW, 0, 'I', "one");
    sl::add(NOW, 0, 'I', "two");
    sl::add(NOW, 0, 'I', "three");
    char out[40];
    CHECK(sl::recent(out, sizeof(out)) == std::strlen(out));
    CHECK(std::string(out) == "12:00:00.000 I three\n");
    char none[8];
    CHECK(sl::recent(none, sizeof(none)) == 0 && none[0] == '\0');
    sl::reset();
}

static void test_size_limit()
{
    sl::reset();
    const std::string dir = paths::logs_dir() + "/big";
    const std::string name = sl::start(dir, NOW, "h\n\n");
    REQUIRE(!name.empty());
    const std::string line(1000, 'x');
    for (int i = 0; i < 1100; i++) sl::add(NOW, 0, 'I', line);
    std::string text = read(dir + "/" + name);
    CHECK(text.size() <= sl::MAX_BYTES + 200);
    CHECK(count(text, "this file stops here") == 1);
    // The end line still goes in.
    sl::end("clean exit");
    CHECK(sl::end_reason(read(dir + "/" + name)) == "clean exit");
    sl::reset();
}

static void test_unfinished()
{
    sl::reset();
    const std::string dir = paths::logs_dir() + "/runs";
    sl::Unfinished u;
    CHECK(!sl::unfinished(dir, "", &u));
    write(dir + "/session_20261013_100000.txt", "h\n\n10:00:00.000 I old\n");   // never ended, reported before
    write(dir + "/session_20261014_100000.txt", "h\n\n10:00:00.000 I a\n=== end: clean exit ===\n");
    CHECK(!sl::unfinished(dir, "", &u));
    write(dir + "/session_20261015_090000.txt", "h\n\n09:00:00.000 I start\n09:00:01.000 E last words\n");
    CHECK(sl::unfinished(dir, "", &u));
    CHECK(u.name == "session_20261015_090000.txt" && u.text.find("last words") != std::string::npos);
    // This run's own file does not count.
    const std::string name = sl::start(dir, NOW, "h\n\n");
    CHECK(sl::unfinished(dir, name, &u) && u.name == "session_20261015_090000.txt");
    // A long one: only its end is read, from a whole line.
    std::string longer = "h\n\n";
    for (int i = 0; i < 2000; i++) longer += "09:00:00.000 I line " + std::to_string(i) + "\n";
    write(dir + "/session_20261015_090000.txt", longer + "09:00:01.000 E last words\n");
    CHECK(sl::unfinished(dir, name, &u));
    CHECK(u.text.size() <= sl::TAIL_BYTES && u.text.compare(0, 13, "09:00:00.000 ") == 0);
    CHECK(sl::last_lines(u.text, 2) == "09:00:00.000 I line 1999\n09:00:01.000 E last words\n");
    write(dir + "/session_20261015_090000.txt", longer + "=== end: clean exit ===\n");
    CHECK(!sl::unfinished(dir, name, &u));
    write(dir + "/session_20261015_090000.txt", "h\n\n09:00:00.000 I start\n09:00:01.000 E last words\n");
    // Ended by a crash: not unfinished.
    write(dir + "/session_20261015_090000.txt", "h\n\n=== end: crash, see logs/crashes/crash_20261015_090001.txt ===\n");
    CHECK(!sl::unfinished(dir, name, &u));
    CHECK(sl::end_reason(read(dir + "/session_20261015_090000.txt")) == "crash, see logs/crashes/crash_20261015_090001.txt");
    sl::reset();

    CHECK(sl::last_lines("a\nb\nc\nd\n", 2) == "c\nd\n");
    CHECK(sl::last_lines("a\nb", 5) == "a\nb");
    CHECK(sl::last_lines("a\nb\n", 0).empty());
    CHECK(sl::end_reason("").empty() && sl::end_reason("\n\n").empty());
    CHECK(sl::end_reason("x\n=== end: clean exit ===") == "clean exit");
}

static void test_crash_files()
{
    sl::reset();
    const std::string crashes = paths::crashes_dir();
    // Not prepared: nothing written.
    CHECK(std::string(crash_report::write(NOW, "x", nullptr)).empty());

    const std::string session = sl::start(paths::sessions_dir(), NOW, "h\n\n");
    crash_report::prepare(crashes, "PlayShelf 9.9.9 (abc1234)", "", session);
    crash_report::prepare(crashes, "PlayShelf 9.9.9 (abc1234)", "firmware 22.5.0, Atmosphère 1.11.1, sysMMC, over a game",
                          session);
    sl::add(NOW, 10, 'I', "opening the activity tab");
    sl::add(NOW, 20, 'E', "something went wrong");

    std::string name = crash_report::write(NOW, "uncaught std::runtime_error: bad", "backtrace:\n  #0 playshelf+0x10");
    CHECK(name == "crash_20261015_120000.txt");
    std::string text = read(crashes + "/" + name);
    CHECK(text.find("2026-10-15 12:00:00  PlayShelf 9.9.9 (abc1234): uncaught std::runtime_error: bad\n") == 0);
    CHECK(text.find("system  : firmware 22.5.0, Atmosphère 1.11.1, sysMMC, over a game\n") != std::string::npos);
    CHECK(text.find("session : logs/sessions/" + session + ", 0 s after the start\n") != std::string::npos);
    CHECK(text.find("\nbacktrace:\n  #0 playshelf+0x10\n") != std::string::npos);
    CHECK(text.find("last lines logged (oldest first):\n12:00:00.010 I opening the activity tab\n"
                    "12:00:00.020 E something went wrong\n") != std::string::npos);
    // The first line keeps the date log_files reads.
    std::string st;
    CHECK(log_files::parse(name, log_files::CRASH, &st) && st == "20261015_120000");

    // Another in the same second, without the last lines.
    name = crash_report::write(NOW, "the run did not end", "last lines of x", false);
    CHECK(name == "crash_20261015_120000_1.txt");
    text = read(crashes + "/" + name);
    CHECK(text.find("last lines logged") == std::string::npos && text.find("last lines of x\n") != std::string::npos);

    // A CPU exception: the dump as the details, then the session ends on it.
    CrashDump d;
    std::memset(&d, 0, sizeof(d));
    d.desc = 0x101;
    d.esr = 0x92000006;
    d.far = 0x10;
    d.module_base = 0x8000000000ULL;
    d.module_size = 0x100000;
    d.pc = d.module_base + 0x4242;
    crash_report_cpu(&d);
    const auto crash_list = log_files::list(crashes, log_files::CRASH);
    REQUIRE(!crash_list.empty());
    text = read(crashes + "/" + crash_list[0]);
    CHECK(text.find("PlayShelf 9.9.9 (abc1234): cpu exception: data abort reading 0x0000000000000010 (a null pointer) "
                    "at playshelf+0x4242\n") != std::string::npos);
    CHECK(text.find("\nregisters:\n") != std::string::npos);
    const std::string run = read(paths::sessions_dir() + "/" + session);
    CHECK(sl::end_reason(run) == "crash, see logs/crashes/" + crash_list[0]);
    // A clean end after it changes nothing.
    sl::end("clean exit");
    CHECK(sl::end_reason(read(paths::sessions_dir() + "/" + session)) == "crash, see logs/crashes/" + crash_list[0]);
    sl::reset();
}

int main()
{
    setenv("TZ", "UTC", 1);
    tzset();
    TempDir tmp("session_log", TempDir::Enter);   // paths::data_dir() is ./playshelf_data on the host

    test_format();
    test_session();
    test_batches();
    test_memory();
    test_size_limit();
    test_unfinished();
    test_crash_files();

    tmp.remove();
    return CHECK_DONE("session log file, memory, size limit, unfinished run and crash file assertions passed");
}
