// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/platform.hpp"

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
}   // namespace

std::string user_agent() { return std::string("PlayShelf/") + app::version(); }

Platform::Platform()
{
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
}

}   // namespace lantern_app
