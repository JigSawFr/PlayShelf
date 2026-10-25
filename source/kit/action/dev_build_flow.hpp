// dev_build_flow — Help & diagnostics › Developer › Install another build: lists the last
// releases, the last commits of main and the open pull requests
// (util/dev_builds.hpp), downloads the one picked, checks it, puts it in
// place of the running .nro and restarts PlayShelf on it through hbloader.
// The list is kept (memory and cache/dev_builds.json) and shown at once for
// ten minutes; Ⓧ fetches it again, a bar on the screen showing the way.
// Find a build looks for the build of any pull request or commit. Its
// screens: activity/dev_builds_activity.hpp (the list) and
// activity/dev_build_activity.hpp (one build, before it is installed).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include "util/dev_builds.hpp"

namespace dev_build_flow
{

// The running .nro from the SD card root ("/switch/playshelf/playshelf.nro"),
// and as a path for the C library; "" when unknown (not started through
// hbloader).
std::string nro_sd_path();
std::string nro_file();

// `force`: fetch the list even when the one kept is fresh.
void open(bool force = false);

// Fetches the list again, without a word when a list or a download is
// already under way (the list's screen, while a build's checks run).
void refresh();

// For the list's screen:
// Whether `b` is the running build: a release by its version (when this one
// is a release build), a commit of main or of a pull request by its commit.
bool installed(const dev_builds::Build& b);
// "Release 1.2.0", "· pre-release" after it for one.
std::string release_text(const dev_builds::Build& b);
// Now, in POSIX seconds (on desktop, PLAYSHELF_SIM_NOW when set).
int64_t now();
// How long ago `iso` was: "just now", "12 min ago", "5 h ago", "3 d ago" ("" when malformed).
std::string age_text(const std::string& iso);
// A commit's or a pull request's subject without its Conventional Commits
// prefix ("chore(main): release 1.3.0" -> "Release 1.3.0"); its commit
// when it has none.
std::string subject(const dev_builds::Build& b);
// "Release 1.2.0", "main · abc1234 · 2026-10-09 12:03", "PR #38 · Subject · abc1234"…
std::string label(const dev_builds::Build& b);
// The running build in `all` (a commit of main or of a pull request), or null.
const dev_builds::Build* running_build(const std::vector<dev_builds::Build>& all);
// "List checked 3 min ago." ("List checked just now." under a minute), for `cache`.
std::string checked_text(const dev_builds::Cache& cache);
// Shows `b` (its date, checks and notes) with Install, which asks for the PIN
// when one is set, then installs it.
// `newer`: it came after the running build.
void choose(const dev_builds::Build& b, bool newer = false);
// Asks for the PIN when one is set, then installs `b` (already confirmed:
// the update screen's Update in place).
void install_now(const dev_builds::Build& b);
// Signs in to GitHub, then lists the builds again.
void sign_in();
// Asks to go back to the previous build (rollback_flow), then the PIN when
// one is set, then goes back to it.
void go_back();
// Find a build: asks for a pull request's number or a commit, looks for its
// build, then shows it (choose()).
void find();

}   // namespace dev_build_flow
