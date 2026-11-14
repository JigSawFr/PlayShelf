// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/platform.hpp"

#include "lantern_app/discovery.hpp"
#include "lantern_app/system.hpp"
#include "library/servers.hpp"
#include "util/paths.hpp"
#include "app.hpp"

#if defined(__SWITCH__)
#include "mbedtls_transport.hpp"
#elif !defined(__EMSCRIPTEN__)
#include "curl_transport.hpp"
#endif

namespace lantern_app
{
namespace
{
// A browser has no sockets to reach a server with.
struct NoNetwork final : lantern::Transport
{
    lantern::Result<void> send(const lantern::Request&, lantern::ResponseHandler&) override
    {
        return lantern::fail(lantern::Category::transport, "no connection to a server from a browser");
    }
};

// The address a server of this uid answers at on the network now, asked by broadcast (discovery.hpp);
// kept as the server's LAN address, so the next start does not ask again. "" when nothing answers.
std::string relocate_by_uid(const std::string& uid)
{
    auto socket = BroadcastSocket::open();
    if (!socket) return "";
    for (const Found& f : discover(*socket, directed_broadcast().value_or(""))) {
        if (f.uid != uid) continue;
        std::vector<library::Server> all = library::load_servers();
        bool                         changed = false;
        for (library::Server& s : all) {
            if (s.uid == uid && s.lan_url != f.url) {
                s.lan_url = f.url;
                changed   = true;
            }
        }
        std::string why;
        if (changed) (void)library::save_servers(all, &why);   // not kept: it is asked again next time
        return f.url;
    }
    return "";
}
}   // namespace

SimHooks s_sim;

void set_sim(SimHooks hooks) { s_sim = std::move(hooks); }

std::string user_agent() { return std::string("PlayShelf/") + app::version(); }

Platform::Platform()
{
    if (s_sim.transport) {
        transport_ = s_sim.transport();
        if (s_sim.store) sim_store_ = s_sim.store();
        env_ = std::make_unique<Env>(Env{ *transport_, trust_, random_, clock_, user_agent(), "switch", "" });
        return;
    }
#if defined(__SWITCH__)
    if (!paths::read_file("romfs:/data/ca-bundle.pem", ca_)) problem_ = "the certificate authorities cannot be read";
    lantern::mbedtls::Options options;
    options.random = &random_;
    options.trust  = &trust_;
    options.ca_pem = ca_;
    auto made      = lantern::mbedtls::MbedTlsTransport::create(options);
    if (made) transport_ = std::make_unique<lantern::mbedtls::MbedTlsTransport>(std::move(*made));
    else problem_ = std::string(made.error().detail());
#elif defined(__EMSCRIPTEN__)
    transport_ = std::make_unique<NoNetwork>();
#else
    lantern::curl::Options options;
    options.trust = &trust_;
    auto made     = lantern::curl::CurlTransport::create(options);
    if (made) transport_ = std::make_unique<lantern::curl::CurlTransport>(std::move(*made));
    else problem_ = std::string(made.error().detail());
#endif
    if (!transport_) transport_ = std::make_unique<NoNetwork>();
    env_ = std::make_unique<Env>(Env{ *transport_, trust_, random_, clock_, user_agent(), "switch", "" });
#if !defined(__EMSCRIPTEN__)
    env_->relocate = relocate_by_uid;
#endif
}

}   // namespace lantern_app
