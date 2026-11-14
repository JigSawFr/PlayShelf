// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/discovery.hpp"

#include "library/catalog.hpp"

namespace lantern_app
{
namespace
{
class Collect final : public lantern::DiscoveryHandler
{
  public:
    std::vector<Found> found;

    bool on_server(const lantern::DiscoveredServer& s) override
    {
        if (!s.reply) return true;
        Found f;
        f.uid  = std::string(s.reply->uid);
        f.name = library::display_text(std::string(s.reply->name), 128);
        f.url  = std::string(s.origin);
        if (f.uid.empty() || !f.url.starts_with("https://")) return true;
        for (Found& old : found)
            if (old.uid == f.uid) {
                old = f;
                return true;
            }
        if (found.size() < DISCOVERY_MAX_SERVERS) found.push_back(std::move(f));
        return found.size() < DISCOVERY_MAX_SERVERS;
    }
};
}   // namespace

std::vector<Found> discover(lantern::UdpSocket& socket, std::string_view directed)
{
    Collect                   collect;
    lantern::DiscoveryOptions options;
    options.address = "255.255.255.255";
    (void)lantern::discover(socket, options, collect);
    if (collect.found.empty() && !directed.empty() && directed != options.address) {
        options.address = directed;
        (void)lantern::discover(socket, options, collect);
    }
    return std::move(collect.found);
}

}   // namespace lantern_app
