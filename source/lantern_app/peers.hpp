// Peers on the local network (SPEC.md § 19.5): paired consoles that hold a
// file of the server already. A reply is a hint: what a peer serves is
// checked by its hash like anything else.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <lantern/discovery.hpp>

namespace lantern_app
{

constexpr size_t PEERS_MAX = 8;

struct PeerAddress
{
    std::string uid, name;
    std::string host;   // the reply's source address
    std::string base;   // "/lantern"
    uint16_t    port = 0;
    bool        tls = false;
};

// The peers whose `servers` hold `server_uid`, as the socket hears them:
// at most PEERS_MAX, each once. A peer that is on this console's own
// network only is the SDK's rule, not ours to relax.
std::vector<PeerAddress> find_peers(lantern::UdpSocket& socket, const std::string& server_uid);

// The peers of `server_uid` on this console's network, asked for at most
// once a minute (SPEC.md L-19.5-2): between two asks the last answer stands.
// Empty when the socket cannot be opened. Safe to call from a thread.
std::vector<PeerAddress> peers_of(const std::string& server_uid);

}   // namespace lantern_app
