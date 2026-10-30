// log_upload — sends a diagnostic report, with the latest crash reports, the
// last runs' session logs and the developer tools' debug files, to a secret gist in the parent's GitHub
// account (the app signed in to GitHub: github_auth.hpp) and gives back its
// link, so a report reaches whoever helps (a bug report, a chat) without
// taking the SD card out. The link also prefills the bug-report form of the
// repository. The gist stays until deleted on GitHub.
// (bpa.st, used before, moved to certificates the console's TLS refuses;
// dpaste.org sits behind Cloudflare, which refuses the Switch.)
//
// What is sent is only what the parent picked, after a confirmation that
// says where it goes: the diagnostic report never holds the PIN or the
// serial number, and the debug files are the app's own settings and the
// last changes of its history, without names or times of day. Anyone with
// the link can read it, and a bug report opened with it is public.
//
// Everything but upload() is plain C++ for the host tests (tests/log_upload);
// upload() is in log_upload_net.cpp.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

namespace log_upload
{

// The most a gist takes from the app. A report is about 30 KiB; the change
// history is what grows.
constexpr size_t GIST_MAX_BYTES = 512 * 1024;

struct Part
{
    std::string name;      // "diagnostic report", "config.json"
    std::string content;
};

// The debug files found in the data directory, in a fixed order:
// history_recent.json (recent_history() of the change history) and
// config.json (the app's settings). The app adds its own
// files before them (PlayGuard's play-timer files).
// Missing ones are left out. Never the GitHub token (github_auth.hpp keeps
// it in a file of its own).
std::vector<Part> debug_files();

// The last `limit` entries of history.json's text (util/history.hpp), oldest
// first as in the file, for a report: what changed and how, without what
// could name someone or tell their day (an entry's detail: an account's,
// game's or profile's name; its time of day: only the date is kept), and how
// many entries there are in all. A history that does not parse is said to
// be so, with its size.
constexpr size_t HISTORY_ENTRIES = 20;
std::string recent_history(const std::string& text, size_t limit = HISTORY_ENTRIES);

// The app's own crash files (logs/crashes/crash_<date>.txt,
// util/crash_report.hpp: a CPU exception, an exception nothing caught, a run
// that never ended), newest first, at most `limit`.
std::vector<Part> crash_files(size_t limit = 3);

// The session files (logs/sessions/session_<date>.txt,
// util/session_log.hpp: everything logged during a run), newest first, at
// most `limit`: this run's and the one before, which a crash ended. Each is
// cut to its header and its last SESSION_BYTES (head_and_tail()).
constexpr size_t SESSION_BYTES = 64 * 1024;
std::vector<Part> session_files(size_t limit = 2);

// `text` when it fits in `max_bytes`, else its header (up to the first blank
// line, when short), a line saying lines were left out, and as many of its
// last whole lines as fit.
std::string head_and_tail(const std::string& text, size_t max_bytes);

// Atmosphère's crash reports, sd:/atmosphere/crash_reports/<POSIX time>_<program id>.log,
// of a homebrew: its process is hbloader, which the app runs as (over a
// game or the Album), so another homebrew's crash can be one of them (the
// confirmation lists them). Those of the `days` days before `now`, newest
// first, at most `limit`, each cut to its first ATMOSPHERE_CRASH_BYTES (the
// crashed thread comes first; the dumps of the other threads after).
constexpr size_t ATMOSPHERE_CRASH_BYTES = 24 * 1024;
std::vector<Part> atmosphere_crashes(std::time_t now, int days = 7, size_t limit = 2);

// Names of the reports saved in logs/reports/ ("report_20261009_141203.txt",
// util/log_files.hpp), newest first, at most `limit`.
std::vector<std::string> saved_reports(size_t limit = 10);

// The parts as the files of a gist, one each, numbered in their order (GitHub
// lists a gist's files by name): "1-report.txt" for a part named without an
// extension (the report), else the number and its path under logs/ with "_"
// for "/" ("2-play_timer_block.json", "1-reports_report_20261009_141203.txt"). An empty part reads
// "(empty)" (GitHub refuses an empty file). At most `max_bytes` in all: the
// part that does not fit is cut on a whole UTF-8 character, with a closing
// notice, the ones after it left out, and *truncated set.
std::vector<Part> gist_files(const std::vector<Part>& parts, size_t max_bytes, bool* truncated = nullptr);

// The bytes of the files' content, together.
size_t total_bytes(const std::vector<Part>& files);

// GitHub's answer to a new gist: true, and *url set, when its html_url is a
// gist.github.com link.
bool parse_gist(const std::string& body, std::string* url);

// A gist's link under its QR code, to read out or type by hand: the address
// without its scheme on a line, then the id in groups of four, so that a
// line breaks between groups ("gist.github.com/u/\nd9ad 36f7 65d7").
std::string link_caption(const std::string& url);

// What the console knows of the bug-report form's fields; empty ones are
// left for the reporter.
struct IssueFields
{
    std::string version;
    std::string firmware;
    std::string atmosphere;
    std::string launcher;   // "over a game (title override)", "from the Album (applet)"
    std::string storage;    // "sysMMC", "emuMMC"
};

// The repository's bug-report form (.github/ISSUE_TEMPLATE/1-bug.yml) with
// the report's link and `fields` filled in. GitHub fills only text fields
// from the link, so the form asks these in text fields, not lists.
std::string issue_url(const std::string& repo_url, const std::string& paste_url, const IssueFields& fields);

// The JSON body of a new secret gist holding `files` (gist_files()),
// described as "<app> <version> <what>" (APP_NAME).
std::string gist_body(const std::vector<Part>& files, const std::string& version,
                      const std::string& what = "diagnostic report");

struct Result
{
    bool        ok = false;
    std::string url;
    std::string error;             // why it failed, in English
    bool        relogin = false;   // GitHub refused the token: sign in again (the gist permission)
};

// Sends `files` (gist_files()) to a secret gist with `token`. Blocking
// (network): run it in the background. On the desktop build
// <APP>_SIM_PASTE ("offline", or the link to answer) replaces the network.
Result upload_gist(const std::vector<Part>& files, const std::string& token, const std::string& version,
                   const std::string& what = "diagnostic report");

}   // namespace log_upload
