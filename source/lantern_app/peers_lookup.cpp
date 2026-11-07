// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <chrono>
#include <mutex>

#include "lantern_app/peers.hpp"
#include "lantern_app/system.hpp"

namespace lantern_app
{

std::vector<PeerAddress> peers_of(const std::string& server_uid)
{
    static std::mutex                              lock;
    static std::string                             last_uid;
    static std::vector<PeerAddress>                last;
    static std::chrono::steady_clock::time_point   asked;
    static bool                                    ever = false;

    const std::lock_guard<std::mutex> guard(lock);
    const auto now = std::chrono::steady_clock::now();
    if (ever && last_uid == server_uid && now - asked < std::chrono::seconds(60)) return last;
    auto socket = BroadcastSocket::open();
    if (!socket) return {};
    last     = find_peers(*socket, server_uid);
    last_uid = server_uid;
    asked    = now;
    ever     = true;
    return last;
}

}   // namespace lantern_app
