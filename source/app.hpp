// app — process-wide state: version, build identity, and the read-only /
// developer modes.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace app
{

// What the app needs from the console before its first screen. Returns true
// on success; the init-error screen says what failed otherwise.
bool init();
void shutdown();
uint32_t    init_result();   // what init() failed with, 0 when it did not

std::string version();            // "1.0.0"
// The commit this build comes from ("1a2b3c4"), empty when unknown (built
// outside a git checkout). Tells which development build is installed.
std::string commit();
// True for a published release (built from its vX.Y.Z tag), false for any
// other build (main, a pull request, a local one): a development build.
bool        is_release();
// The pull request this build was made for by CI (306), 0 for any other build.
int         pr();
// Where the running .nro is, from the SD card root ("/switch/playshelf/playshelf.nro"),
// as hbloader gave it in argv[0]; empty when unknown (nxlink, desktop).
std::string self_path();
void        set_self_path(const char* argv0);
const char* repo_url();
// A page of the user guide (docs/<page>.md), with an anchor: a release's as
// published at its tag, main's for a development build.
std::string doc_url(const std::string& page, const std::string& anchor = "");
// Funding links (.github/FUNDING.yml), shown in About.
constexpr const char* SPONSORS_URL = "https://github.com/sponsors/JigSawFr";
constexpr const char* KOFI_URL     = "https://ko-fi.com/jigsawfr";

// Read-only mode: the service layer refuses every change (core/write_guard.h)
// and the tabs hide their write controls.
// After a change, call ui::on_mode_changed() to update what is on screen.
bool        read_only();
void        set_read_only(bool on);

// Developer mode: the developer tools at the end of About › Help & diagnostics.
// Saved in the preferences, or on for this session only.
bool        dev_mode();
// Saved off the UI thread, with ui::save_config's toasts.
void        set_dev_mode(bool on, bool persist, const std::string& saved_text = "", const std::string& failed_text = "");

// False while the app is in the background (HOME menu, a system applet …):
// periodic refreshes skip their work then.
bool        in_focus();

}   // namespace app
