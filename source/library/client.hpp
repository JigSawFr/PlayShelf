// client — a library server's catalogue fetched for a profile (Server): its
// address chosen (the LAN one when it answers, by the profile's
// connection), the credentials sent (HTTP Basic, over TLS only), the API
// asked (/api/remote/sections and /api/shop/sections, the whole catalogue
// rather than AeroFoil's first few hundred games, then the older
// /api/remote and /api/shop; Ownfoil 2's GraphQL API, a page at a time,
// and its handshake) or the address read as a JSON index (its
// "directories" followed, a few levels down), the answer parsed
// (catalog.hpp).
//
// TLS: a WAN address is vouched for by the CA store; a LAN one by the pin
// the profile keeps (the SHA-256 of its server's public key). A LAN server
// whose certificate the CA store refuses and whose profile has no pin yet
// ends the fetch as UnknownCertificate, with the pin of the key it showed:
// the user compares it with the server's and, trusting it, has it kept (pin
// on first use), then fetches again. A pin that no longer matches fails.
//
// The network is a Transport (http_transport.cpp; the tests give their own).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

#include "library/catalog.hpp"
#include "library/server.hpp"
#include "util/http.hpp"

namespace library
{

enum class Format : uint8_t { Auto, Api, Index };

// A server as the user set it up (servers.hpp keeps them).
struct Server
{
    std::string id;     // 16 hex digits, made once
    std::string name;
    std::string lan_url, wan_url;   // https:// each, one at least
    Connection  connection = Connection::Auto;
    Format      format = Format::Auto;
    std::string username;
    std::string password_box;   // util/secret_box: sealed for this console
    std::string pin;            // "sha256//…": the LAN server's key, once trusted
};

struct Answer
{
    long        status = 0;   // 0: no answer
    std::string body;
    bool        tls_failed = false;   // the certificate or key refused
    std::string error;
    std::vector<std::string> cookies;   // call(): "name=value" each
    std::string              location;  // call(): a redirect not followed
};

class Transport
{
  public:
    virtual ~Transport() = default;
    virtual Answer get(const std::string& url, const std::vector<std::string>& headers, const std::string& pin) = 0;
    // Any other request (a method, a form, a file sent or written to):
    // http::call's. Unless a transport makes it, a plain GET goes to get()
    // and anything else is not sent ("not supported").
    virtual Answer call(const std::string& url, const http::Call& c, const std::vector<std::string>& headers,
                        const std::string& pin);
    // Whether `url`'s host answers at all (a LAN address tried first).
    virtual bool reachable(const std::string& url) = 0;
    // The certificate `url` presents, as PEM; "" with `why` when it cannot be read.
    virtual std::string certificate(const std::string& url, std::string* why) = 0;
};

enum class FetchStatus { Ok, Failed, UnknownCertificate };

// Which API answered: AeroFoil's sections (its other services, services.hpp,
// go with it), an older /api/remote or /api/shop, Ownfoil 2's GraphQL, or
// none (an index).
enum class Dialect : uint8_t { None, Sections, Shop, GraphQl };

struct Fetch
{
    FetchStatus status = FetchStatus::Failed;
    std::string why;
    Catalog     catalog;
    std::string endpoint;   // the address used
    bool        on_lan = false;
    Format      format = Format::Auto;   // the one that answered (Api or Index)
    Dialect     dialect = Dialect::None;
    ServerInfo  info;                     // GraphQl: its handshake, when it answered
    // UnknownCertificate: the key the LAN server showed.
    std::string offered_pin;     // pin_text()
    std::string offered_short;   // pin_short(), for the user to compare
};

// What AeroFoil is asked for at most in a section ("?limit="): without it,
// its "All" stops at a few hundred games.
constexpr size_t SECTIONS_LIMIT = MAX_ITEMS;

constexpr int    MAX_INDEX_DEPTH = 3;
constexpr size_t MAX_INDEX_FETCHES = 16;

Fetch fetch(const Server& server, const std::string& password, Transport& transport);

// Whether `a` and `b` are on the same server: scheme, host and port alike
// (case aside, :443 the same as none). What a catalogue's item is sent
// with depends on it: the credentials and the pin go only to the server
// they are for, never to another host an item points at.
bool same_origin(const std::string& a, const std::string& b);

// "Authorization: Basic …" for `user` and `password`; nothing for no user.
std::vector<std::string> auth_headers(const std::string& user, const std::string& password);

}   // namespace library
