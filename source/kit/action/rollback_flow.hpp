// rollback_flow — going back to the previous build (util/rollback.hpp).
// At start-up, before borealis (so a build that crashes in its UI still
// gets there): when the last start of a build just installed never reached
// its first screen, a text screen offers the previous build; A asks for
// the PIN (the system's screen, when one is set), swaps the two .nro files
// and starts it again through hbloader, B goes on. At any other time,
// Install another build has a line that does the same, after the PIN.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace rollback_flow
{

// The start-up check, first thing in main(). True when PlayShelf must end
// now (the previous build is in place, hbloader starts it).
bool at_start();

// The first screen is up: the start of the build installed went well (the
// notification bell says it was installed).
void started();

// The main screen: when the start-up went back to the previous build (or
// Install another build did), the notification bell says so, once.
void note_went_back();

// The running build's name: "1.3.0 (#318, c3fadd3)" (a pull request's),
// "1.3.0 (8b1e468)", "1.3.0" (a release's).
std::string running_label();

// A build was installed in place of the running one: the running one's
// name is kept as the previous build's, and the next start is checked.
void installed(const std::string& installed_label);

// The previous build's name, when there is one to go back to.
bool previous(std::string* label);

// Goes back to it now: swaps the files, then restarts on it through
// hbloader (or says to restart by hand). On the UI thread; the caller asked
// for the confirmation and the PIN.
void go_back_now();

}   // namespace rollback_flow
