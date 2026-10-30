// rollback — going back to the build the app ran before the last one
// installed in place (Install another build, Update in place). Installing
// keeps the replaced .nro as "<nro>.previous" and its name in
// previous_build.json; starting a build just installed is checked
// (boot_check.json): a start that never reached its first screen (it
// crashed) offers the previous build at the next start, before anything
// else. The screen that
// asks is action/rollback_flow.hpp's; this is the plain C++ part, for the
// host tests (tests/rollback).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace rollback
{

// previous_build.json, boot_check.json and went_back.json's "schema". They
// are read by the build gone back to, older by design: a key is only ever
// added (one changing meaning would be a new key), and their readers take
// any schema, skipping what they do not know.
constexpr int SCHEMA = 1;

// "<nro>.previous": the build that ran before.
std::string previous_file(const std::string& nro);

// What previous_build.json names ("1.2.0", "1.2.0 (abc1234)"); false when
// there is no such file, or it is damaged.
bool read_label(const std::string& json_file, std::string* label);
bool write_label(const std::string& json_file, const std::string& label);

// The start of a build just installed, under way (boot_check.json):
// `starts` is how many starts began without reaching the first screen.
struct Check
{
    std::string label;   // the build installed
    int         starts = 0;
};
bool read_check(const std::string& json_file, Check* out);
bool write_check(const std::string& json_file, const Check& check);

// Whether the previous build is offered at start-up: the last start of the
// build installed did not reach its first screen. `checked`: boot_check.json
// was there, `starts_before` read from it (before this start counts);
// `have_previous`: <nro>.previous is there. It is offered before
// the app's own lock screen (which may be what crashes); going back still
// needs the PIN (go_back_allowed).
bool offer_at_start(bool checked, int starts_before, bool have_previous);

// Whether the start-up may go back once the system's PIN screen (pctlauth,
// not the app's code) was shown: the PIN entered, or no PIN set. A start
// that did not finish is easy to cause (the power button), and going back
// replaces the app, as Install another build does with the PIN.
bool go_back_allowed(bool pin_entered, bool no_pin_set);

// Swaps <nro> and <nro>.previous (the build running becomes the previous
// one), through paths::replace_file. When a step fails, what was moved is
// put back.
[[nodiscard]] bool swap(const std::string& nro, std::string* error);

// An install or a swap stopped half-way (a power cut between its renames,
// paths::recover_replace): <nro> put back from "<nro>.old", or the build
// replaced kept as <nro>.previous as it should have been. At start-up, and
// before each swap or install.
void recover(const std::string& nro);

}   // namespace rollback
