// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/session.hpp"

#include "lantern_app/catalog_map.hpp"

namespace lantern_app
{
namespace
{
constexpr uint32_t APPS_PER_PAGE = 200;
constexpr int      MAX_PAGES = 1000;   // MAX_ITEMS at APPS_PER_PAGE, with room

std::string without_slash(std::string u)
{
    while (!u.empty() && u.back() == '/') u.pop_back();
    return u;
}

lantern::ClientConfig config_of(const Env& env, const std::string& origin)
{
    lantern::ClientConfig c;
    c.origin          = origin;
    c.user_agent      = env.user_agent;
    c.platform        = env.platform;
    c.accept_language = env.language;
    c.random          = &env.random;
    c.clock           = &env.clock;
    c.trust           = &env.trust;
    return c;
}

// The trust for a server of this uid: the key it pinned, else the
// authorities' word.
void prepare_trust(Env& env, const std::string& uid, lantern::CredentialStore& store)
{
    env.trust.require_ca();
    if (uid.empty()) return;
    const auto kept = store.load(uid, {});
    if (kept && *kept && (*kept)->spki) env.trust.pin(*(*kept)->spki);
}
}   // namespace

std::string describe(const lantern::Error& e)
{
    const std::string_view cat = lantern::category_name(e.category());
    std::string            out(cat);
    if (!e.detail().empty()) out += ": " + std::string(e.detail());
    else if (e.status() != 0) out += ": HTTP " + std::to_string(e.status());
    return out;
}

std::string choose(const library::Server& server, Env& env, lantern::CredentialStore& store)
{
    const library::Endpoints e{server.lan_url, server.wan_url, server.connection};
    return without_slash(library::choose_endpoint(e, [&](const std::string& url) {
        prepare_trust(env, server.uid, store);
        auto client = lantern::Client::create(env.transport, config_of(env, without_slash(url)));
        if (!client) return false;
        const auto shake = client->handshake();
        return shake.has_value();
    }));
}

Connected connect(const library::Server& server, Env& env, lantern::CredentialStore& store)
{
    Connected c;
    c.endpoint = choose(server, env, store);
    if (!c.endpoint.starts_with("https://")) {
        c.why = "the server has no https:// address";
        return c;
    }
    c.on_lan = !server.lan_url.empty() && c.endpoint == without_slash(server.lan_url);

    prepare_trust(env, server.uid, store);
    auto created = lantern::Client::create(env.transport, config_of(env, c.endpoint));
    if (!created) {
        c.why = describe(created.error());
        return c;
    }
    c.client.emplace(std::move(*created));
    const auto shake = c.client->handshake();
    if (!shake) {
        c.why = describe(shake.error());
        c.client.reset();
        return c;
    }
    if (!*shake) {
        c.why = "the server speaks a newer version of Lantern than this one reads";
        c.client.reset();
        return c;
    }
    const lantern::Handshake& hs = **shake;
    c.name = library::display_text(std::string(hs.name), 128);
    c.uid  = std::string(hs.uid);
    if (!server.uid.empty() && c.uid != server.uid) {
        c.why = "this is another server than the one that was paired";
        c.client.reset();
        return c;
    }
    const auto signed_in = c.client->sign_in(store);
    if (!signed_in) {
        c.why = describe(signed_in.error());
        c.client.reset();
        return c;
    }
    if (!*signed_in) {
        c.status = Status::NotPaired;
        c.why    = "this console is not paired with the server";
        c.client.reset();
        return c;
    }
    c.status = Status::Ok;
    return c;
}

Fetched fetch(const library::Server& server, Env& env, lantern::CredentialStore& store)
{
    Fetched f;
    Connected c = connect(server, env, store);
    f.status   = c.status;
    f.why      = c.why;
    f.endpoint = c.endpoint;
    f.on_lan   = c.on_lan;
    f.name     = c.name;
    f.uid      = c.uid;
    if (c.status != Status::Ok) return f;
    f.status = Status::Failed;
    lantern::Client& client = *c.client;
    f.catalog.motd = library::display_text(std::string(client.server()->motd), 512);
    f.cheats   = client.offers(lantern::Family::cheats) > 0;
    f.saves    = client.offers(lantern::Family::saves) > 0;
    f.requests = client.offers(lantern::Family::requests) > 0;

    AppIndex index;
    lantern::AppsQuery query;
    query.page_size = APPS_PER_PAGE;
    std::string cursor;
    for (int page = 0; page < MAX_PAGES; page++) {
        query.cursor = cursor;
        const auto apps = client.apps(query);
        if (!apps) {
            f.why = describe(apps.error());
            return f;
        }
        if (!*apps) {
            f.why = "the server lists its apps in a newer version than this one reads";
            return f;
        }
        add_apps(f.catalog, index, (*apps)->items, f.endpoint);
        cursor.assign((*apps)->next);
        if (cursor.empty() || f.catalog.truncated) break;
    }
    if (!cursor.empty()) f.catalog.truncated = true;

    if (client.offers(lantern::Family::sections) > 0) {
        if (const auto sections = client.sections(); sections && *sections)
            add_sections(f.catalog, index, (*sections)->sections);
    }
    f.status = Status::Ok;
    return f;
}

lantern::Result<Pairing> begin_pairing(const std::string& endpoint, bool on_lan, const std::string& device, Env& env)
{
    // A LAN server vouched for by no authority: its key is taken, shown
    // next to the code and pinned once the code is given (§ 5.3).
    if (on_lan) env.trust.allow_unverified();
    else env.trust.require_ca();
    auto created = lantern::Client::create(env.transport, config_of(env, without_slash(endpoint)));
    if (!created) return std::unexpected(created.error());
    Pairing p;
    p.client   = std::make_unique<lantern::Client>(std::move(*created));
    p.endpoint = endpoint;
    p.on_lan   = on_lan;
    const auto shake = p.client->handshake();
    if (!shake) return std::unexpected(shake.error());
    if (!*shake) return lantern::fail(lantern::Category::unsupported, "a newer version of Lantern than this one reads");
    p.uid  = std::string((*shake)->uid);
    p.name = library::display_text(std::string((*shake)->name), 128);

    const std::string_view scopes[] = {"library", "saves", "cheats", "requests"};
    lantern::PairingOptions opt;
    opt.device = device;
    opt.scopes = scopes;
    auto started = p.client->start_pairing(opt);
    if (!started) return std::unexpected(started.error());
    p.pairing         = std::move(*started);
    p.code            = std::string(p.pairing.code.code);
    p.verify          = std::string(p.pairing.code.verify);
    p.verify_complete = std::string(p.pairing.code.verify_complete);
    p.fingerprint     = std::string(p.pairing.fingerprint_view());
    return p;
}

lantern::Result<bool> poll_pairing(Pairing& p, lantern::CredentialStore& store)
{
    const auto state = p.client->poll_pairing(p.pairing, &store);
    if (!state) return std::unexpected(state.error());
    return *state == lantern::PollState::paired;
}

}   // namespace lantern_app
