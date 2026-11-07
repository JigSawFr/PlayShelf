// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/peers.hpp"

#include "library/catalog.hpp"

namespace lantern_app
{
namespace
{
class Collect final : public lantern::DiscoveryHandler
{
  public:
    explicit Collect(const std::string& uid_) : uid(uid_) {}

    std::vector<PeerAddress> found;

    // Servers are not asked for here.
    bool on_server(const lantern::DiscoveredServer&) override { return true; }

    bool on_peer(const lantern::DiscoveredPeer& p) override
    {
        if (!p.reply || p.reply->port == 0 || p.reply->port > 65535 || !lantern::peer_serves(*p.reply, uid)) return true;
        PeerAddress a;
        a.uid  = std::string(p.reply->uid);
        a.name = library::display_text(std::string(p.reply->name), 128);
        a.host = std::string(p.endpoint.host);
        a.base = std::string(p.endpoint.base);
        a.port = (uint16_t)p.endpoint.port;
        a.tls  = p.endpoint.tls;
        if (a.uid.empty() || a.host.empty()) return true;
        for (const PeerAddress& old : found)
            if (old.uid == a.uid) return true;
        found.push_back(std::move(a));
        return found.size() < PEERS_MAX;
    }

  private:
    std::string uid;
};
}   // namespace

std::vector<PeerAddress> find_peers(lantern::UdpSocket& socket, const std::string& server_uid)
{
    Collect                   collect(server_uid);
    lantern::DiscoveryOptions options;
    options.address = "255.255.255.255";
    options.peers   = true;
    (void)lantern::discover(socket, options, collect);
    return std::move(collect.found);
}

}   // namespace lantern_app
