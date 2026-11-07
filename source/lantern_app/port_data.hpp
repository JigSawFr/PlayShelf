// The game data a port needs from the user's own copy (SPEC.md 8.6): what
// the SD card holds of it, read through the library's DataProbe, as lines a
// screen can show. Plain C++, host-tested (tests/lantern_ports).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

#include <lantern/lantern.hpp>

namespace lantern_app
{

struct PortData
{
    bool                     is_port = false;   // the entry is a port at all
    bool                     ready = false;     // every required file is there, of a known version
    bool                     unverified = false;   // a file was taken on its presence alone
    std::vector<std::string> missing;   // "path (hint)" for each file to bring
    std::string              error;     // the SD card could not be read
};

// The data of `entry` under `sd_root`. An entry that is not a port gives
// `is_port == false`.
PortData check_port_data(const lantern::Homebrew& entry, const std::string& sd_root);

}   // namespace lantern_app
