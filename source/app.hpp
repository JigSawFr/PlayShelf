// app — process-wide state: version, build identity, and the read-only /
// developer modes.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

#include "kit_app.hpp"   // version(), commit(), repo_url(), read_only(), dev_mode(): the kit asks for them

namespace app
{

// What the app needs from the console before its first screen. Returns true
// on success; the init-error screen says what failed otherwise.
bool init();
void shutdown();
uint32_t    init_result();   // what init() failed with, 0 when it did not

// is_release(), pr() and self_path() (kit_app.hpp) are defined here.
void        set_self_path(const char* argv0);
// The tab another Boréale homebrew opened PlayShelf at (a click in its bell:
// --open=<tab>, boreale-link's bl_open_route): one of the main screen's
// tabs (activity/tabs.hpp), "" none or one PlayShelf does not know.
std::string open_route();
void        set_open_route(int argc, char** argv);
// A page of the user guide (docs/<page>.md), with an anchor: a release's as
// published at its tag, main's for a development build.
std::string doc_url(const std::string& page, const std::string& anchor = "");

// Read-only mode (kit_app.hpp): the tabs hide their write controls.
// After a change, call ui::on_mode_changed() to update what is on screen.
void        set_read_only(bool on);

// Developer mode (kit_app.hpp): the developer tools at the end of About › Help & diagnostics.
// Saved off the UI thread, with ui::save_config's toasts.
void        set_dev_mode(bool on, bool persist, const std::string& saved_text = "", const std::string& failed_text = "");

// False while the app is in the background (HOME menu, a system applet …):
// periodic refreshes skip their work then.
bool        in_focus();

}   // namespace app
