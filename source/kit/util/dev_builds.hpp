// dev_builds — install another build of the app in place, from the
// developer tools: one of the last releases, one of the last commits of main, or
// the newest build of an open pull request (from this repository: a fork's
// build is never offered, nobody with write access has reviewed its code).
//
// A release is the <app>.nro (APP_ID, app_id.h) of a GitHub release
// (pre-releases included, marked), readable without an account. The others
// are the <app>-<version>-<ref>-<commit>-nro
// artifact the build workflow keeps for every run (.github/workflows/build.yml
// names each artifact of a run after its build and its component; the
// recovery sysmodule is the -rescue one): a zip holding <app>.nro, which
// GitHub only hands to a signed-in user, hence the token of
// util/github_auth.hpp. Artifacts expire (90 days by default).
//
// The download is checked (size and the SHA-256 GitHub records; for an
// artifact, the zip's, then the CRC-32 of <app>.nro inside it; the NRO
// header) before it replaces the running .nro, which hbloader loaded whole
// into memory, so it can be overwritten once romfs:/ lets go of it
// (platform_romfs_release, dev_build_flow.cpp); then hbloader starts it.
//
// Everything but fetch() and download() is plain C++ for the host tests
// (tests/dev_builds); those two are in dev_builds_net.cpp.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <functional>

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace dev_builds
{

enum class Kind
{
    Release,       // a release (its <app>.nro)
    Main,          // a commit of main
    PullRequest,   // the newest build of an open pull request
    Commit,        // a commit found by Find a build (any branch of this repository)
};

// How the build workflow's run that made an artifact ended.
enum class Ci
{
    Unknown,   // not listed (an older run), or not an artifact
    Running,   // the artifact is there, the run is still going (tests…)
    Passed,
    Failed,    // failed or cancelled
};

struct Build
{
    Kind        kind = Kind::Main;
    bool        artifact = false;   // url, size and sha256 are those of a zip holding <app>.nro
    std::string version;   // Release: "1.2.0"
    bool        prerelease = false;   // Release: marked a pre-release on GitHub
    int         pr = 0;    // PullRequest: its number
    std::string title;     // PullRequest: its title
    std::string branch;    // Commit: its branch
    std::string commit;    // Main / PullRequest / Commit: 7 hex digits
    std::string date;      // "2026-10-09T12:03:00Z", when it was built
    std::string url;       // the download (an artifact's archive_download_url needs the token)
    uint64_t    size = 0;
    std::string sha256;    // 64 hex digits; empty when GitHub gave no digest (not installed, then)
    std::string notes;     // Release: its notes; PullRequest: its description (Markdown, at most MAX_NOTES bytes)
    std::string expires;   // artifact: when GitHub deletes it ("2027-01-07T12:03:00Z")
    std::string page;      // Release / PullRequest: its page on GitHub ("https://github.com/…"; else empty)
    int64_t     run = 0;   // artifact: the workflow run that made it
    Ci          ci = Ci::Unknown;   // artifact: how that run ended
};

// One run's artifact, as GET /repos/…/actions/artifacts lists it.
struct Artifact
{
    std::string sha256, url, date, branch, commit;   // commit: the run's head, 7 hex digits
    std::string expires;
    int64_t     run = 0;   // the workflow run's id
    uint64_t    size = 0;
    int64_t     repo = 0, head_repo = 0;   // the run's repository and the head's (a fork's for its PRs)
};

// An open pull request (GET /repos/…/pulls).
struct Pull
{
    int         number = 0;
    std::string title, branch, body;   // body: at most MAX_NOTES bytes, cut on a line
    std::string page;                  // its page on GitHub
    std::string head_sha;              // full
    int64_t     head_repo = 0, repo = 0;   // the head's repository (a fork's) and this one
};

// One run of the build workflow (GET /repos/…/actions/workflows/build.yml/runs).
struct Run
{
    int64_t     id = 0;
    std::string sha;   // full
    std::string title;     // its commit's subject (display_title), clean
    std::string message;   // its commit's whole message (head_commit.message), as notes
    Ci          ci = Ci::Unknown;
    int64_t     repo = 0, head_repo = 0;
    std::string branch;
};

// Everything shown from GitHub's answers (titles, notes, branches) goes
// through util/safe_text.hpp first, and a release's version must be one
// (safe_text::is_version): nothing GitHub sends can hide or reorder text.

// The latest release (GET /repos/…/releases/latest): its <app>.nro and
// its notes (at most MAX_NOTES bytes, cut on a line). False for anything
// else (a pre-release, no such asset, a tag that is not a version, malformed).
constexpr size_t MAX_NOTES = 16 * 1024;
bool parse_release(const std::string& json, Build* out);
// The releases (GET /repos/…/releases), newest first as listed: each one with
// an <app>.nro, pre-releases included (Build::prerelease), drafts left
// out, at most `keep`, one per version. False when the answer is not a list.
constexpr size_t MAX_RELEASES = 20;
bool parse_releases(const std::string& json, std::vector<Build>* out, size_t keep = MAX_RELEASES);
// The artifact holding <app>.nro: "<app>-<version>-<ref>-<commit>-nro",
// or "<app>_release", the name the runs before that scheme gave it.
bool is_build_artifact(const std::string& name);
// The build artifacts (is_build_artifact) that have not expired, newest first
// as listed. *listed (when given) is how many artifacts the page held, of
// any name: a short page is the last one.
bool parse_artifacts(const std::string& json, std::vector<Artifact>* out, size_t* listed = nullptr);
bool parse_pulls(const std::string& json, std::vector<Pull>* out);
// One pull request (GET /repos/…/pulls/<n>).
bool parse_pull(const std::string& json, Pull* out);
// The build workflow's runs ({"workflow_runs": […]}): "completed" with
// "success" passes, any other conclusion fails; not completed, Running.
bool parse_runs(const std::string& json, std::vector<Run>* out);
// Sets the CI state of each artifact build from its run in `runs`, and a
// commit of main its title (the commit's subject) and notes (its message).
void apply_runs(std::vector<Build>* builds, const std::vector<Run>& runs);

// A commit's or a pull request's title in the Conventional Commits form
// ("feat(ui)!: text (#143)"), split: type "feat", scope "ui", breaking, the
// text "text" and the pull request it came from, 143. A title in no such
// form is all text.
struct Subject
{
    std::string type, scope, text;
    bool        breaking = false;
    int         pr = 0;
};
Subject parse_subject(const std::string& title);
// A commit (GET /repos/…/commits/<ref>): its full hash.
bool parse_commit(const std::string& json, std::string* sha);

// What Find a build was given: "38" or "#38" (a pull request), or 7 to 40
// hex digits (a commit). Spaces around are ignored.
struct Query
{
    int         pr = 0;
    std::string commit;   // lower case
};
bool parse_query(const std::string& text, Query* out);

// The newest build artifact of `artifacts` (a run's), as a build of `kind`;
// false when none is left (expired, another artifact).
bool newest_build(const std::vector<Artifact>& artifacts, Kind kind, Build* out);

// Whether `b` came after the running build: a release with a higher version
// (`running_version`), an artifact built after the running one (`running`,
// the running commit's build in `all`, when it is listed there).
bool newer(const Build& b, const std::string& running_version, const Build* running);

// "2026-10-09T12:03:00Z" -> "2026-10-09 12:03"; a shorter text as it is.
std::string short_date(const std::string& iso);

// How long before `now` (POSIX seconds) the time `iso` ("…T…Z") was, as the
// builds' screens say it: under a minute (or later than now) Now, under an
// hour in whole minutes, under two days in whole hours, else in whole days;
// None when `iso` is malformed.
struct Age
{
    enum Unit { None, Now, Minutes, Hours, Days } unit = None;
    int count = 0;   // the minutes, the hours or the days
};
Age age_of(const std::string& iso, int64_t now);

// Whole days from `now` (POSIX seconds) to the time `iso` ("…T…Z"), rounded
// down: negative once it is past; 0 when `iso` is malformed.
int days_left(const std::string& iso, int64_t now);

// The builds: main's newest `keep_main` commits (pushed to this repository's
// main), then for each open pull request from a branch of this repository
// the newest artifact of that branch (pull requests from forks are left out). Newest first within each kind; one per commit.
std::vector<Build> combine(const std::vector<Artifact>& artifacts, const std::vector<Pull>& pulls,
                           size_t keep_main = 20);

// How Help & diagnostics › Developer › Install another build orders main's commits and the
// pull requests (config.json "dev_builds_order").
enum class Order
{
    Newest,   // the newest build first
    Number,   // pull requests by number, the highest first (main: newest first)
};
// "newest" or "number"; anything else is Newest.
Order order_named(const std::string& name);
const char* order_name(Order order);

// The builds of `kind` in `all`, in `order`; builds that tie keep the order
// they are listed in.
std::vector<Build> of_kind(const std::vector<Build>& all, Kind kind, Order order);

// "sha256:<64 hex>" (GitHub's digest) -> the 64 hex digits, lower case;
// empty when malformed.
std::string digest_hex(const std::string& digest);

// An NRO: "NRO0" at offset 0x10.
bool is_nro(const std::string& head);

// Checks a downloaded file against `size` and `sha256`. A file without a
// digest to check it against is refused (the error NO_DIGEST): the size and
// the NRO header say nothing of what it holds. NO_DIGEST is a marker the
// screens compare against and say as dev_build/no_digest, in the user's
// language.
extern const char* const NO_DIGEST;
// Done, else why not (in English).
using Done = std::expected<void, std::string>;

[[nodiscard]] Done verify_file(const std::string& path, uint64_t size, const std::string& sha256);
// Checks that `path` is an NRO.
[[nodiscard]] Done verify_nro(const std::string& path);

// Replaces `target` with `fresh` (both on the same file system), through
// paths::replace_file: target moves aside to "<target>.old", fresh takes
// its name, then the old one is removed, or kept as `keep_as` when given
// (util/rollback.hpp: the previous build). When fresh cannot take its place,
// the old one is put back. A missing target is simply created.
[[nodiscard]] Done replace(const std::string& target, const std::string& fresh, const std::string& keep_as = "");

// The last list fetched, kept on the SD card (cache/dev_builds.json) so the
// next opening shows it at once while it is fresh.
struct Cache
{
    std::vector<Build> builds;
    bool    needs_login = false;   // as fetch() set it: signing in or out makes the list stale
    int64_t fetched_at = 0;        // POSIX seconds
};

// A list older than this is fetched again before it is shown.
constexpr int64_t CACHE_MAX_AGE_S = 10 * 60;

// cache/dev_builds.json's "schema" (named "version" before: still read).
constexpr int CACHE_SCHEMA = 1;

std::string encode_cache(const Cache& cache);
// False for anything else (damaged, another schema: fetched again); a build that does not
// look like one (a URL not on https://github.com/ or https://api.github.com/,
// a malformed commit, a digest missing or malformed) is left out.
bool decode_cache(const std::string& json, Cache* out);
// Fresh: fetched less than CACHE_MAX_AGE_S before `now` (not in the future:
// the clock was set back), in the same sign-in state.
bool cache_fresh(const Cache& cache, int64_t now, bool needs_login);

// What fetch() is doing: its share of the work done (0..1) and the step.
enum class Step
{
    Releases,
    Artifacts,
    Pulls,
    Runs,
};
using Progress = std::function<void(float done, Step step)>;

// The builds that can be installed: the last releases, then (with a token)
// main and the pull requests, with how their runs ended. Blocking (network):
// run it with aurore::async; `progress` (when set) is called from that thread.
// *needs_login is set when there is no token, so only the releases are listed,
// whether or not the fetch succeeds.
// On the desktop build <APP_ENV>_SIM_DEV_BUILDS ("offline", or a folder
// holding releases.json, artifacts.json and pulls.json; latest.json for
// fetch_release) replaces the network.
[[nodiscard]] std::expected<std::vector<Build>, std::string> fetch(bool* needs_login, Progress progress = nullptr);

// Find a build: the newest build of pull request `q.pr` (open or closed), or
// of commit `q.commit`, from a branch of this repository (a fork's is
// refused). Needs the token. Blocking (network); simulated like fetch() with
// pull_<n>.json, commit.json, runs.json and run_artifacts.json.
[[nodiscard]] std::expected<Build, std::string> find(const Query& q);

// The latest release alone (no account needed): what Update in place
// installs. Blocking (network); simulated like fetch().
[[nodiscard]] std::expected<Build, std::string> fetch_release();

// Downloads `b` to `path` (<app>.nro itself, out of the zip for an
// artifact) and verifies it. Blocking. On the desktop build, with
// <APP_ENV>_SIM_DEV_BUILDS set, a URL "https://<path>" is a local file.
// `progress` (when set) gets the bytes received and the total, from the
// calling thread.
// `cancel` (when given), set by another thread, stops it within a second
// (the error "cancelled").
[[nodiscard]] Done download(const Build& b, const std::string& path,
                            std::function<void(uint64_t done, uint64_t total)> progress = nullptr,
                            const std::atomic<bool>* cancel = nullptr);

}   // namespace dev_builds
