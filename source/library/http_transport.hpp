// http_transport — library::Transport over the kit's HTTP (util/http.hpp):
// a GET with the profile's pin (any other request too: a method, a form, a
// file sent or written to), a quick probe of a LAN address, the
// certificate a LAN server presents. And the LAN discovery itself: the
// OWNFOIL_DISCOVER broadcast on UDP port 8465, the answers gathered for 3
// seconds (server.hpp reads each one).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "library/client.hpp"

namespace library
{

class HttpTransport : public Transport
{
  public:
    Answer      get(const std::string& url, const std::vector<std::string>& headers, const std::string& pin) override;
    Answer      call(const std::string& url, const http::Call& c, const std::vector<std::string>& headers,
                     const std::string& pin) override;
    bool        reachable(const std::string& url) override;
    std::string certificate(const std::string& url, std::string* why) override;
};

// The servers that answered on this network; `why` set when the broadcast
// could not be sent. Stops early when `cancel` is set.
std::vector<Discovered> discover(const std::atomic<bool>* cancel, std::string* why);

}   // namespace library
