// update — is there a newer PlayShelf, and does it support this firmware?
//
// Every release publishes compat.json next to playshelf.nro (written by
// tools/gen_compat.py from CMakeLists.txt and core/sysinfo.h):
//   {"schema": 1, "version": "1.1.0", "fw_tested_max": "24.0.0"}
// Parsing and the decision are plain C++ (host-tested); check() fetches the
// file of the latest release over HTTPS (update_check.cpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace update
{

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
// Reads compat.json; false when a required field is missing or malformed.
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

// Fetches compat.json of the latest release and decides. Blocking (network):
// run it with aurore::async. On the desktop build PLAYSHELF_SIM_LATEST
// ("1.1.0:24.0.0", "none" or "offline") replaces the network.
Result      check(uint32_t console_fw);

}   // namespace update
