// server — the parts of reaching a library server that need no network:
// a LAN discovery answer (Ownfoil's and AeroFoil's "OWNFOIL" reply to the
// OWNFOIL_DISCOVER broadcast on UDP port 8465), which address of a profile
// to use (its LAN one when it answers, else its WAN one), and the pin of a
// server's certificate (the SHA-256 of its public key, SPKI, as libcurl's
// CURLOPT_PINNEDPUBLICKEY takes it), read from the certificate itself.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace library
{

constexpr uint16_t    DISCOVERY_PORT = 8465;
constexpr const char* DISCOVERY_REQUEST = "OWNFOIL_DISCOVER";
constexpr size_t      DISCOVERY_MAX_REPLY = 4096;
constexpr size_t      DISCOVERY_MAX_SERVERS = 32;

struct Discovered
{
    std::string uid;       // a UUID, lower case
    std::string name, version;
    std::string address;   // the IPv4 address the answer came from
    uint16_t    port = 0;
    bool        is_public = false;   // reachable without an account
    std::string remote;    // its WAN address, when it gives one
    // The address PlayShelf would use: https:// only.
    std::string url() const { return "https://" + address + ":" + std::to_string(port); }
};

// An answer from `source_ip` (dotted IPv4): nothing when it is not a
// well-formed OWNFOIL reply, too big, or from a broadcast or multicast
// address.
std::optional<Discovered> parse_discovery(const char* data, size_t size, const std::string& source_ip);
// `s` into `servers`: replaces the one of the same uid, else added (at most
// DISCOVERY_MAX_SERVERS).
void add_discovered(std::vector<Discovered>& servers, const Discovered& s);

enum class Connection : uint8_t { Lan, Wan, Auto };

struct Endpoints
{
    std::string lan_url, wan_url;
    Connection  connection = Connection::Auto;
};

// The address to use: Lan or Wan as set (the other one when that one is
// empty); Auto the LAN one when `answers` says it answers, else the WAN one.
std::string choose_endpoint(const Endpoints& e, const std::function<bool(const std::string& url)>& answers);

// The SHA-256 of a certificate's SubjectPublicKeyInfo, from the
// certificate's DER bytes; nothing when they are not an X.509 certificate.
std::optional<std::array<uint8_t, 32>> spki_sha256(const uint8_t* der, size_t size);
// The DER bytes of the first certificate of a PEM text.
std::optional<std::vector<uint8_t>> pem_to_der(const std::string& pem);
// "sha256//<base64>", what CURLOPT_PINNEDPUBLICKEY takes.
std::string pin_text(const std::array<uint8_t, 32>& spki_hash);
// A pin for a person to compare: "AB:CD:…" (the first 8 bytes).
std::string pin_short(const std::array<uint8_t, 32>& spki_hash);

std::string base64(const uint8_t* data, size_t size);
std::optional<std::vector<uint8_t>> unbase64(const std::string& text);

}   // namespace library
