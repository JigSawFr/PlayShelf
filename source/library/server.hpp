// server — which address of a server to use, and whether two addresses are
// of one origin: the LAN address when it answers, else the WAN one.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace library
{

enum class Connection : uint8_t { Lan, Wan, Auto };

struct Endpoints
{
    std::string lan_url, wan_url;
    Connection  connection = Connection::Auto;
};

// The address to use: Lan or Wan as set (the other one when that one is
// empty); Auto the LAN one when `answers` says it answers, else the WAN one.
std::string choose_endpoint(const Endpoints& e, const std::function<bool(const std::string& url)>& answers);

// Whether two addresses are of one origin (scheme, host, port; the default
// https port left out).
bool same_origin(const std::string& a, const std::string& b);

}   // namespace library
