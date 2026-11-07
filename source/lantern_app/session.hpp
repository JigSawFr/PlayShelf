// A Lantern server read from this console: the address chosen (LAN or WAN),
// the key pinned at pairing, the handshake, the apps and their sections
// as a library::Catalog. The platform gives the transport (libcurl on a PC,
// mbedTLS on the console), its trust, a random source and a clock.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <lantern/lantern.hpp>

#include "lantern_app/catalog_map.hpp"
#include "library/catalog.hpp"
#include "library/servers.hpp"

namespace lantern_app
{

// What the platform provides; `trust` is the one the transport was made with.
struct Env
{
    lantern::Transport&   transport;
    lantern::ServerTrust& trust;
    lantern::RandomSource& random;
    lantern::Clock&       clock;
    std::string           user_agent;
    std::string           platform = "switch";
    std::string           language;   // Accept-Language, "" for none
};

enum class Status { Ok, Failed, NotPaired };

struct Fetched
{
    Status          status = Status::Failed;
    std::string     why;
    library::Catalog catalog;
    std::string     endpoint;   // the address used
    bool            on_lan = false;
    std::string     name;       // the server's own (handshake)
    std::string     uid;
    bool            cheats = false, saves = false, requests = false;   // what the server offers besides packages
    std::vector<ServerTitle> titles;   // its sheets for the games of the catalogue (the first MAX_SHEETS)
};

// "category: detail" of an SDK error, for a person.
std::string describe(const lantern::Error& e);

// The address to use: the one set, or in Auto the LAN one when its
// handshake answers, else the WAN one. "" when there is none.
std::string choose(const library::Server& server, Env& env, lantern::CredentialStore& store);

// A client for `server`, signed in: its address chosen, the key pinned at
// pairing trusted, its handshake read (the uid must be the paired one) and
// this console's token loaded. `fetched` tells why not: NotPaired when no
// token is kept for its uid.
struct Connected
{
    Status                       status = Status::Failed;
    std::string                  why;
    std::optional<lantern::Client> client;
    std::string                  endpoint;
    bool                         on_lan = false;
    std::string                  name, uid;
};
Connected connect(const library::Server& server, Env& env, lantern::CredentialStore& store);

// The apps and sections of a paired server. NotPaired when no token is kept
// for its uid (it was never paired, or the credentials were erased).
Fetched fetch(const library::Server& server, Env& env, lantern::CredentialStore& store);

// A pairing in progress: the code to enter on the server's page, the
// address to open (a QR code carries `verify_complete`), the key it showed
// for a person to compare.
struct Pairing
{
    std::unique_ptr<lantern::Client> client;
    lantern::Pairing                 pairing;
    std::string                      code, verify, verify_complete, fingerprint;
    std::string                      uid, name, endpoint;
    bool                             on_lan = false;
};

// Opens a pairing with the server at `endpoint` (a self-signed LAN server's
// key is taken and shown, pinned once paired).
lantern::Result<Pairing> begin_pairing(const std::string& endpoint, bool on_lan, const std::string& device, Env& env);
// One poll: true once paired (the credentials are in `store`).
lantern::Result<bool> poll_pairing(Pairing& p, lantern::CredentialStore& store);

}   // namespace lantern_app
