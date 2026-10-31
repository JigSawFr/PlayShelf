// update — is there a newer build of the app, and does it support this firmware?
//
// Every release publishes compat-<COMPAT_SCHEMA>.json next to the app's .nro,
// and compat.json in schema 1 for the versions before it (written by
// tools/gen_compat.py from CMakeLists.txt and core/sysinfo.h):
//   {"schema": 1, "version": "1.1.0", "fw_tested_max": "24.0.0"}
// (an app may add its own fields: they are read where it reads them).
// Parsing and the decision are plain C++ (host-tested); check() fetches the
// file of the latest release over HTTPS (update_check.cpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace update
{

// The compat document's "schema" this version reads (tools/gen_compat.py's SCHEMA).
constexpr int COMPAT_SCHEMA = 1;

struct Latest
{
    std::string version;            // "1.1.0"
    uint32_t    fw_tested_max = 0;  // MAKEHOSVERSION layout: major << 16 | minor << 8 | micro
};

enum class Verdict
{
    Unknown,           // could not tell (offline, release without compat.json, bad file)
    UpToDate,          // this is already the newest release
    NoRelease,         // nothing at releases/latest (404): no release published yet
    UpdateSupports,    // a newer release supports this console's firmware
    UpdateNoSupport,   // a newer release exists, but not for this firmware yet
};

struct Result
{
    Verdict     verdict = Verdict::Unknown;
    Latest      latest;
    std::string error;   // why the check failed (Unknown only), in English
};

// "1.2.3" (optionally "v1.2.3" or "1.2.3-dev") -> major, minor, micro.
bool     parse_version(const std::string& text, int out[3]);
// <0, 0 or >0 like strcmp. A fourth number counts ("1.6.2.3" > "1.6.2",
// sys-patch's tags); a suffixed version ("1.0.0-dev") sorts before the
// plain release with the same numbers; unparsable versions sort first.
int      compare_versions(const std::string& a, const std::string& b);
// "24.0.0" -> MAKEHOSVERSION(24, 0, 0); 0 when malformed.
uint32_t parse_firmware(const std::string& text);
// Reads compat.json; false when a required field is missing or malformed, or
// when its "schema" is newer than COMPAT_SCHEMA (a missing one is 1).
bool     parse_compat(const std::string& json, Latest* out);
Verdict  decide(uint32_t console_fw, const std::string& app_version, const Latest& latest);

// The start-up screen that offers a newer release (action/update_flow):
// "Later" postpones it for a version, at most SNOOZE_MAX_DAYS days.
constexpr int SNOOZE_MAX_DAYS = 30;

struct Snooze
{
    std::string version;   // the release postponed
    std::string until;     // "YYYY-MM-DD": offered again from that day
};

// Whether start-up offers `latest`: a version newer than `running`, not
// postponed. A postponement is for its version only (a newer release is
// offered at once), and ignored when its day is over SNOOZE_MAX_DAYS away
// (the clock was set back, or a hand-edited file).
bool offer_at_start(const std::string& latest, const std::string& running, const Snooze& snooze,
                    const std::string& today);

// Fetches compat-<COMPAT_SCHEMA>.json of the latest release (compat.json
// when the release has none, or one this version cannot read) and decides. Blocking (network):
// run it with aurore::async. On the desktop build <APP_ENV>_SIM_LATEST
// ("1.1.0:24.0.0", "none" or "offline") replaces the network.
Result      check(uint32_t console_fw);

// The newest release of another Boréale homebrew, from its repository
// ("owner/name" on GitHub: its compat document, as check() reads PlayGuard's),
// for the update check made for all (action/update_flow); "" when it cannot
// tell. Blocking (network). On the desktop build <APP_ENV>_SIM_LATEST_OF
// ("owner/name=1.2.0;…") replaces the network.
std::string latest_of(const std::string& repo);

}   // namespace update
