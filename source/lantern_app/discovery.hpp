// Servers found on the local network (SPEC.md § 4.1): a broadcast on UDP
// port 47115, each answer giving the server's uid, name and port. The
// address is the answer's source; https:// only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

#include <lantern/discovery.hpp>

namespace lantern_app
{

constexpr size_t DISCOVERY_MAX_SERVERS = 32;

struct Found
{
    std::string uid, name;
    std::string url;   // https://<address>:<port>
};

// What `socket` hears: a server of an uid already found is kept once.
std::vector<Found> discover(lantern::UdpSocket& socket);

}   // namespace lantern_app
