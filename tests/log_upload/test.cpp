// Host tests for source/util/log_upload.cpp: which debug files and saved
// reports are offered, what of the change history goes, the crash reports sent, a gist's files and their cut
// on a whole UTF-8 character, GitHub's answer, the body sent and the
// prefilled bug-report link (percent-encoded: text::url_encode, tests/text_util).
#include "check.h"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/log_upload.hpp"
#include "util/paths.hpp"

static void write(const std::string& path, const std::string& content)
{
    CHECK(paths::atomic_write(path, content));
}

static void test_files()
{
    CHECK(log_upload::debug_files().empty());
    CHECK(log_upload::saved_reports().empty());

    CHECK(paths::ensure_dir(paths::logs_dir()));
    write(paths::config_file(), "{\"theme\": \"dark\"}");
    auto files = log_upload::debug_files();
    CHECK(files.size() == 1 && files[0].name == "config.json");
    write(paths::history_file(), "[]");
    files = log_upload::debug_files();
    CHECK(files.size() == 2 && files[0].name == "history_recent.json" && files[1].name == "config.json");
    CHECK(files[0].content.find("could not be read") != std::string::npos);   // not the shape history.json has
    // The GitHub token (util/github_auth.hpp) is never sent.
    write(paths::data_dir() + "/github_token", "ghu_secret");
    files = log_upload::debug_files();
    CHECK(files.size() == 2);
    for (const auto& f : files) CHECK(f.content.find("ghu_secret") == std::string::npos);

    write(paths::reports_dir() + "/report_20261008_090000.txt", "a");
    write(paths::reports_dir() + "/report_20261009_141203.txt", "b");
    write(paths::reports_dir() + "/report_20261009_141203_1.txt", "c");
    write(paths::reports_dir() + "/notes.txt", "not a report");
    write(paths::reports_dir() + "/report_2026100_x.txt", "not a report");
    write(paths::crashes_dir() + "/crash_20261010_090000.txt", "a crash, not a report");
    auto reports = log_upload::saved_reports();
    CHECK(reports.size() == 3);
    CHECK(reports[0] == "report_20261009_141203_1.txt" && reports[1] == "report_20261009_141203.txt" &&
           reports[2] == "report_20261008_090000.txt");
    reports = log_upload::saved_reports(1);
    CHECK(reports.size() == 1 && reports[0] == "report_20261009_141203_1.txt");
}

static void test_crashes()
{
    // PlayShelf's own, newest first, at most the limit (test_files wrote the newest).
    write(paths::crashes_dir() + "/crash_20261009_090000.txt", "old");
    write(paths::crashes_dir() + "/crash_20261010_090000.txt", "a crash, not a report");
    write(paths::crashes_dir() + "/notes.txt", "not a crash");
    auto crashes = log_upload::crash_files();
    CHECK(crashes.size() == 2 && crashes[0].name == "logs/crashes/crash_20261010_090000.txt" &&
          crashes[0].content == "a crash, not a report" && crashes[1].content == "old");
    CHECK(log_upload::crash_files(1).size() == 1);

    // Atmosphère's: a homebrew's (hbloader) of the last days, newest first.
    const std::time_t now = 1791569454;
    CHECK(log_upload::atmosphere_crashes(now).empty());
    const std::string dir = paths::sd_root() + "/atmosphere/crash_reports";
    CHECK(paths::ensure_dir(dir));
    const std::string hbl = "Atmosphère Crash Report (v1.1):\nCrashed Process Info:\n"
                            "    Program ID:                      010000000000100d\n"
                            "    Process Name:                    hbloader\n";
    write(dir + "/01791569000_010000000000100d.log", hbl + "newest");
    write(dir + "/01791500000_0100000000001000.log", hbl + "older");
    write(dir + "/01791560000_0100000000001000.log",
          "Crashed Process Info:\n    Process Name:                    qlaunch\n");   // not a homebrew
    write(dir + "/01790000000_010000000000100d.log", hbl + "too old");
    write(dir + "/01791600000_010000000000100d.log", hbl + "later than now");
    write(dir + "/report.txt", hbl);
    write(dir + "/x_010000000000100d.log", hbl);
    auto ams = log_upload::atmosphere_crashes(now);
    REQUIRE(ams.size() == 2);
    CHECK(ams[0].name == "atmosphere/crash_reports/01791569000_010000000000100d.log" &&
          ams[0].content == hbl + "newest");
    CHECK(ams[1].content == hbl + "older");
    CHECK(log_upload::atmosphere_crashes(now, 7, 1).size() == 1);
    CHECK(log_upload::atmosphere_crashes(now, 0).empty());
    // A large one: its start only, cut on a whole character.
    write(dir + "/01791569400_010000000000100d.log", hbl + std::string(100000, 'x'));
    ams = log_upload::atmosphere_crashes(now, 7, 1);
    REQUIRE(ams.size() == 1);
    CHECK(ams[0].content.size() <= log_upload::ATMOSPHERE_CRASH_BYTES &&
          ams[0].content.find("(cut: too large to send whole)") != std::string::npos);
}

static void test_sessions()
{
    CHECK(log_upload::session_files().empty());
    write(paths::sessions_dir() + "/session_20261009_090000.txt", "oldest");
    write(paths::sessions_dir() + "/session_20261010_090000.txt", "before\n=== end: crash, see x ===\n");
    write(paths::sessions_dir() + "/session_20261011_090000.txt", "this run");
    write(paths::sessions_dir() + "/notes.txt", "not a session");
    auto files = log_upload::session_files();
    REQUIRE(files.size() == 2);
    CHECK(files[0].name == "logs/sessions/session_20261011_090000.txt" && files[0].content == "this run");
    CHECK(files[1].content == "before\n=== end: crash, see x ===\n");

    // A long one: its header and its last whole lines.
    std::string text = "=== PlayShelf session ===\nbuild   : x\n\n";
    for (int i = 0; i < 5000; i++) text += "12:00:00.000 I line " + std::to_string(i) + "\n";
    text += "=== end: clean exit ===\n";
    const std::string kept = log_upload::head_and_tail(text, 4096);
    CHECK(kept.size() <= 4096 && kept.size() > 3000);
    CHECK(kept.compare(0, 40, "=== PlayShelf session ===\nbuild   : x\n\n(") == 0);
    CHECK(kept.find("(... the lines in between left out ...)\n12:00:00.000 I line ") != std::string::npos);
    CHECK(kept.find("I line 4999\n=== end: clean exit ===\n") != std::string::npos);
    CHECK(log_upload::head_and_tail("short", 4096) == "short");
    // No blank line: no header, the end only.
    const std::string flat(10000, 'x');
    const std::string cut = log_upload::head_and_tail(flat + "\nlast\n", 100);
    CHECK(cut.compare(0, 9, "(... the ") == 0 && cut.size() <= 100);
    write(paths::sessions_dir() + "/session_20261012_090000.txt", text);
    files = log_upload::session_files(1);
    REQUIRE(files.size() == 1);
    CHECK(files[0].content.size() <= log_upload::SESSION_BYTES);
    CHECK(files[0].content == log_upload::head_and_tail(text, log_upload::SESSION_BYTES));

    // A large one (the part in between is not read): the same as from the
    // whole text, header and last lines.
    std::string big = "=== PlayShelf session ===\nbuild   : x\n\n";
    for (int i = 0; i < 40000; i++) big += "12:00:00.000 I a longer line, number " + std::to_string(i) + "\n";
    big += "=== end: clean exit ===\n";
    REQUIRE(big.size() > 1000 * 1000);
    write(paths::sessions_dir() + "/session_20261013_090000.txt", big);
    files = log_upload::session_files(1);
    REQUIRE(files.size() == 1);
    CHECK(files[0].content == log_upload::head_and_tail(big, log_upload::SESSION_BYTES));
    CHECK(files[0].content.compare(0, 40, "=== PlayShelf session ===\nbuild   : x\n\n(") == 0);

    // paths::read_ends: the whole file when small, else its two ends; a
    // missing file read from its ".tmp".
    std::string part;
    const std::string small = paths::sessions_dir() + "/ends.txt";
    write(small, "0123456789");
    CHECK(paths::read_ends(small, 4, 6, part) && part == "0123456789");
    CHECK(paths::read_ends(small, 3, 2, part) && part == "01289");
    CHECK(paths::read_ends(small, 0, 3, part) && part == "789");
    CHECK(paths::read_ends(small, 4, 0, part) && part == "0123");
    CHECK(std::rename(small.c_str(), (small + ".tmp").c_str()) == 0);
    CHECK(paths::read_ends(small, 2, 0, part) && part == "01");
    CHECK(std::remove((small + ".tmp").c_str()) == 0);
    CHECK(!paths::read_ends(small, 2, 2, part));
}

static void test_gist_files()
{
    bool cut = true;
    // One file each, numbered in order: the report (a name without an
    // extension) as report.txt, the others by their path under logs/.
    const std::vector<log_upload::Part> parts{ { "diagnostic report", "report\n" },
                                               { "logs/play_timer/block.json", "{}" },
                                               { "logs/play_timer/log.csv", "" },
                                               { "history.json", "[]" } };
    const std::vector<log_upload::Part> f = log_upload::gist_files(parts, 1000, &cut);
    CHECK(!cut);
    REQUIRE(f.size() == 4);
    CHECK(f[0].name == "1-report.txt" && f[0].content == "report\n");
    CHECK(f[1].name == "2-play_timer_block.json" && f[1].content == "{}");
    CHECK(f[2].name == "3-play_timer_log.csv" && f[2].content == "(empty)\n");   // GitHub refuses an empty file
    CHECK(f[3].name == "4-history.json" && f[3].content == "[]");
    CHECK(log_upload::total_bytes(f) == 7 + 2 + 8 + 2);
    // A saved report keeps its own name; a translated label is still the report.
    CHECK(log_upload::gist_files({ { "logs/reports/report_20261009_141203.txt", "x" } }, 100, &cut)[0].name ==
          "1-reports_report_20261009_141203.txt");
    CHECK(log_upload::gist_files({ { "rapport de diagnostic", "x" } }, 100, &cut)[0].name == "1-report.txt");
    CHECK(log_upload::gist_files({}, 100, &cut).empty() && !cut);

    // Too large: the part that does not fit is cut (with the notice), the
    // ones after it left out, never more than the limit in all.
    const std::string big(5000, 'x');
    const std::vector<log_upload::Part> c =
        log_upload::gist_files({ { "report", "abc" }, { "history.json", big }, { "config.json", "{}" } }, 200, &cut);
    CHECK(cut);
    REQUIRE(c.size() == 2);
    CHECK(c[0].content == "abc");
    CHECK(c[1].name == "2-history.json" && c[1].content.find("(cut: too large to send whole)") != std::string::npos);
    CHECK(log_upload::total_bytes(c) <= 200);
    // No room left for a cut part: left out.
    const std::vector<log_upload::Part> d = log_upload::gist_files({ { "report", big }, { "config.json", "{}" } }, 5000, &cut);
    CHECK(cut && d.size() == 1 && d[0].content == big);

    // A cut never splits a character: "é" is two bytes.
    std::string accents;
    for (int i = 0; i < 200; i++) accents += "\xC3\xA9";
    for (size_t limit = 60; limit < 120; limit++) {
        const std::vector<log_upload::Part> e = log_upload::gist_files({ { "fr", accents } }, limit, &cut);
        REQUIRE(cut && e.size() == 1);
        CHECK(e[0].content.size() <= limit);
        CHECK(e[0].content.find("\n\n(cut") % 2 == 0);
    }
}

static void test_reply()
{
    std::string url;
    CHECK(log_upload::parse_gist(R"({"id": "aa5a", "html_url": "https://gist.github.com/aa5a315d61ae9438b18d"})",
                                  &url));
    CHECK(url == "https://gist.github.com/aa5a315d61ae9438b18d");
    CHECK(log_upload::parse_gist(R"({"html_url": "https://gist.github.com/octo-cat/aa5a"})", &url) &&
           url == "https://gist.github.com/octo-cat/aa5a");
    url = "kept";
    const char* bad_gist[] = { "", "not json", "[]", R"({"message": "Not Found"})", R"({"html_url": 5})",
                               R"({"html_url": "https://gist.github.com/"})",
                               R"({"html_url": "http://gist.github.com/aa5a"})",
                               R"({"html_url": "https://gist.github.com.evil.example/aa5a"})",
                               R"({"html_url": "https://gist.github.com//aa5a"})",
                               R"({"html_url": "https://gist.github.com/aa5a/"})",
                               R"({"html_url": "https://gist.github.com/aa5a?x=1"})" };
    for (const char* b : bad_gist) {
        CHECK(!log_upload::parse_gist(b, &url));
        CHECK(url == "kept");
    }
}

static void test_bodies()
{
    CHECK(log_upload::gist_body({ { "1-report.txt", "a \"b\"\n\xC3\xA9" }, { "2-config.json", "{}" } }, "1.2.0") ==
           R"({"description":"PlayShelf 1.2.0 diagnostic report","files":{"1-report.txt":{"content":"a \"b\"\n)"
           "\xC3\xA9"
           R"("},"2-config.json":{"content":"{}"}},"public":false})");
    // Another kind of gist: its own description.
    CHECK(log_upload::gist_body({ { "1-games.txt", "x" } }, "1.2.0", "games to reinstall") ==
           R"({"description":"PlayShelf 1.2.0 games to reinstall","files":{"1-games.txt":{"content":"x"}},"public":false})");
}

static void test_urls()
{
    CHECK(log_upload::link_caption("https://gist.github.com/JigSawFr/d9ad36f765d7e0e791f20202a7d93712") ==
           "gist.github.com/JigSawFr/\nd9ad 36f7 65d7 e0e7 91f2 0202 a7d9 3712");
    CHECK(log_upload::link_caption("https://gist.github.com/aa5a315") == "gist.github.com/\naa5a 315");
    CHECK(log_upload::link_caption("gist.github.com") == "gist.github.com");
    log_upload::IssueFields fields;
    CHECK(log_upload::issue_url("https://github.com/o/r", "https://gist.github.com/aa5a", fields) ==
           "https://github.com/o/r/issues/new?template=1-bug.yml&report=https%3A%2F%2Fgist.github.com%2Faa5a");
    fields.version = "1.2.0";
    fields.firmware = "23.0.1";
    fields.atmosphere = "1.12.0";
    fields.launcher = "from the Album (applet)";
    fields.storage = "emuMMC";
    CHECK(log_upload::issue_url("https://github.com/o/r", "https://gist.github.com/u/ab12", fields) ==
           "https://github.com/o/r/issues/new?template=1-bug.yml&report=https%3A%2F%2Fgist.github.com%2Fu%2Fab12"
           "&version=1.2.0&firmware=23.0.1&atmosphere=1.12.0"
           "&launcher=from%20the%20Album%20%28applet%29&storage=emuMMC");
}

// The last entries of the change history, without names or times of day.
static void test_recent_history()
{
    std::string text = "{\n \"schema\": 1,\n \"entries\": [";
    for (int i = 0; i < 30; i++)
        text += std::string(i ? ",\n  " : "\n  ") + R"({"when":"2026-10-)" + (i < 9 ? "0" : "") + std::to_string(i + 1) +
                R"( 18:30","kind":"remove_game","source":"remove","detail":"Secret Game )" + std::to_string(i) +
                R"x( (0100000000001000)","version":"1.2.0"})x";
    text += R"(,
  {"when":"2026-10-31 07:05","kind":"limits","source":"day","detail":"","before":[60,60,60,60,60,60,60],"after":[60,90,60,60,60,60,60],"result":"0x0001088E"},
  {"when":"2026-10-31 07:06","kind":"new_account","source":"","detail":"Alice"}
 ]
})";
    const std::string out = log_upload::recent_history(text);
    CHECK(out.find("\"entries_in_all\": 32") != std::string::npos);
    CHECK(out.find("Secret Game") == std::string::npos && out.find("Alice") == std::string::npos);
    CHECK(out.find("detail") == std::string::npos && out.find("18:30") == std::string::npos &&
          out.find("07:05") == std::string::npos);
    // The last 20, oldest first: from the 13th of the 30 removals.
    CHECK(out.find("2026-10-12") == std::string::npos && out.find("\"2026-10-13\"") != std::string::npos);
    CHECK(out.find("\"2026-10-13\"") < out.find("\"2026-10-31\""));
    // Values and the console's refusal are what a report is for: kept.
    CHECK(out.find(R"("after":[60,90,60,60,60,60,60])") != std::string::npos &&
          out.find(R"("result":"0x0001088E")") != std::string::npos && out.find(R"("version":"1.2.0")") != std::string::npos);
    size_t entries = 0;
    for (size_t at = out.find("\"kind\""); at != std::string::npos; at = out.find("\"kind\"", at + 1)) entries++;
    CHECK(entries == log_upload::HISTORY_ENTRIES);
    CHECK(log_upload::recent_history(text, 1).find("\"kind\":\"new_account\"") != std::string::npos);

    CHECK(log_upload::recent_history(R"({"schema":1,"entries":[]})") == "{\n \"entries_in_all\": 0,\n \"entries\": []\n}\n");
    CHECK(log_upload::recent_history("{not json").find("could not be read: 9 bytes") != std::string::npos);
    // An entry of another shape is passed on as it is (still nothing else of the file).
    CHECK(log_upload::recent_history(R"({"entries":[3,{"when":5}]})").find(R"("when":5)") != std::string::npos);
}

int main()
{
    TempDir tmp("log_upload", TempDir::Enter);   // paths::data_dir() is ./playshelf_data on the host

    test_files();
    test_recent_history();
    test_crashes();
    test_sessions();
    test_gist_files();
    test_reply();
    test_bodies();
    test_urls();

    tmp.remove();
    return CHECK_DONE("log upload files, recent history, crashes, gist files, gist answer and body, bug-report link assertions passed");
}
