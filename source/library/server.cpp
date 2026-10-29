// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/server.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include <aurore/extern/nlohmann/json.hpp>

#include "library/catalog.hpp"
#include "util/sha256.hpp"

namespace library
{
namespace
{
bool is_uuid(const std::string& s)
{
    if (s.size() != 36) return false;
    for (size_t i = 0; i < s.size(); i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return false;
        } else if (!std::isxdigit((unsigned char)s[i])) {
            return false;
        }
    }
    return true;
}

// Dotted IPv4 as a number ("192.168.1.20": four decimal numbers up to 255,
// nothing else); nothing when it is not one.
std::optional<uint32_t> ipv4(const std::string& s)
{
    uint32_t addr = 0;
    size_t   i = 0;
    for (int part = 0; part < 4; part++) {
        if (part && (i >= s.size() || s[i++] != '.')) return std::nullopt;
        const size_t start = i;
        uint32_t     v = 0;
        while (i < s.size() && i - start < 3 && std::isdigit((unsigned char)s[i]))
            v = v * 10 + (uint32_t)(s[i++] - '0');
        if (i == start || v > 255) return std::nullopt;
        addr = addr << 8 | v;
    }
    if (i != s.size()) return std::nullopt;
    return addr;
}

// A DER element at [*at, end): its tag, and where its value starts and ends.
struct Tlv
{
    uint8_t tag = 0;
    size_t  start = 0, value = 0, end = 0;   // the element's first byte, its value's, past its last
};
std::optional<Tlv> next(const uint8_t* d, size_t at, size_t limit)
{
    if (at + 2 > limit) return std::nullopt;
    Tlv t;
    t.start = at;
    t.tag = d[at++];
    size_t len = d[at++];
    if (len & 0x80) {
        const size_t n = len & 0x7f;
        if (n == 0 || n > 4 || at + n > limit) return std::nullopt;
        len = 0;
        for (size_t i = 0; i < n; i++)
            len = len << 8 | d[at++];
    }
    if (len > limit - at) return std::nullopt;
    t.value = at;
    t.end = at + len;
    return t;
}
}   // namespace

std::optional<Discovered> parse_discovery(const char* data, size_t size, const std::string& source_ip)
{
    using json = nlohmann::json;
    if (!data || !size || size > DISCOVERY_MAX_REPLY) return std::nullopt;
    const auto ip = ipv4(source_ip);
    if (!ip || *ip == 0 || (*ip >> 24) >= 224 || (*ip & 0xff) == 0xff) return std::nullopt;
    bool       deep = false;
    const json j = json::parse(
        data, data + size,
        [&](int depth, json::parse_event_t, json&) {
            if (depth > 8) deep = true;
            return depth <= 8;
        },
        false);
    if (deep || j.is_discarded() || !j.is_object()) return std::nullopt;
    for (const char* f : {"magic", "uid", "name", "version"})
        if (!j.contains(f) || !j[f].is_string()) return std::nullopt;
    if (j["magic"] != "OWNFOIL" || !j.contains("public") || !j["public"].is_boolean() || !j.contains("port") ||
        !j["port"].is_number_integer())
        return std::nullopt;
    const int64_t port = j["port"].get<int64_t>();
    if (port < 1 || port > 65535) return std::nullopt;
    Discovered s;
    s.uid = j["uid"].get<std::string>();
    if (!is_uuid(s.uid)) return std::nullopt;
    std::transform(s.uid.begin(), s.uid.end(), s.uid.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    s.name = display_text(j["name"].get<std::string>(), 48);
    s.version = display_text(j["version"].get<std::string>(), 24);
    if (j.contains("remote")) {
        if (!j["remote"].is_string()) return std::nullopt;
        std::string r = j["remote"].get<std::string>();
        // Ownfoil gives its host alone ("shop.example.org"): over https.
        if (r.find("://") == std::string::npos && !r.empty() &&
            std::all_of(r.begin(), r.end(), [](unsigned char c) { return std::isalnum(c) || c == '.' || c == '-' || c == ':'; }))
            r = "https://" + r;
        // An address it may become: kept whole or not at all, and https only.
        if (r.size() <= 512 && r.rfind("https://", 0) == 0) s.remote = r;
    }
    s.address = source_ip;
    s.port = (uint16_t)port;
    s.is_public = j["public"].get<bool>();
    return s;
}

void add_discovered(std::vector<Discovered>& servers, const Discovered& s)
{
    const auto it = std::find_if(servers.begin(), servers.end(), [&](const Discovered& o) { return o.uid == s.uid; });
    if (it != servers.end())
        *it = s;
    else if (servers.size() < DISCOVERY_MAX_SERVERS)
        servers.push_back(s);
}

std::string choose_endpoint(const Endpoints& e, const std::function<bool(const std::string&)>& answers)
{
    if (e.lan_url.empty()) return e.wan_url;
    if (e.wan_url.empty()) return e.lan_url;
    switch (e.connection) {
    case Connection::Lan: return e.lan_url;
    case Connection::Wan: return e.wan_url;
    case Connection::Auto: return answers && answers(e.lan_url) ? e.lan_url : e.wan_url;
    }
    return e.wan_url;
}

std::optional<std::array<uint8_t, 32>> spki_sha256(const uint8_t* der, size_t size)
{
    // Certificate ::= SEQUENCE { tbsCertificate SEQUENCE { [0] version?,
    // serialNumber, signature, issuer, validity, subject,
    // subjectPublicKeyInfo, … }, … }
    const auto cert = next(der, 0, size);
    if (!cert || cert->tag != 0x30 || cert->end != size) return std::nullopt;
    const auto tbs = next(der, cert->value, cert->end);
    if (!tbs || tbs->tag != 0x30) return std::nullopt;
    size_t     at = tbs->value;
    auto       field = next(der, at, tbs->end);
    if (field && field->tag == 0xa0) {   // the version
        at = field->end;
        field = next(der, at, tbs->end);
    }
    // serialNumber (INTEGER), then four SEQUENCEs: signature, issuer, validity, subject.
    if (!field || field->tag != 0x02) return std::nullopt;
    at = field->end;
    for (int i = 0; i < 4; i++) {
        field = next(der, at, tbs->end);
        if (!field || field->tag != 0x30) return std::nullopt;
        at = field->end;
    }
    const auto spki = next(der, at, tbs->end);
    if (!spki || spki->tag != 0x30) return std::nullopt;
    Sha256 h;
    h.update(der + spki->start, spki->end - spki->start);
    return h.digest();
}

std::string base64(const uint8_t* data, size_t size)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string       out;
    for (size_t i = 0; i < size; i += 3) {
        const uint32_t v = (uint32_t)data[i] << 16 | (i + 1 < size ? (uint32_t)data[i + 1] << 8 : 0) |
                           (i + 2 < size ? data[i + 2] : 0);
        out += T[v >> 18 & 63];
        out += T[v >> 12 & 63];
        out += i + 1 < size ? T[v >> 6 & 63] : '=';
        out += i + 2 < size ? T[v & 63] : '=';
    }
    return out;
}

std::optional<std::vector<uint8_t>> unbase64(const std::string& text)
{
    std::vector<uint8_t> out;
    uint32_t             acc = 0;
    int                  bits = 0, pad = 0;
    for (char c : text) {
        int v;
        if (c >= 'A' && c <= 'Z')
            v = c - 'A';
        else if (c >= 'a' && c <= 'z')
            v = c - 'a' + 26;
        else if (c >= '0' && c <= '9')
            v = c - '0' + 52;
        else if (c == '+')
            v = 62;
        else if (c == '/')
            v = 63;
        else if (c == '=') {
            pad++;
            continue;
        } else if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
            continue;
        else
            return std::nullopt;
        if (pad) return std::nullopt;   // data after the padding
        acc = acc << 6 | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(acc >> bits));
        }
    }
    if (pad > 2) return std::nullopt;
    return out;
}

std::optional<std::vector<uint8_t>> pem_to_der(const std::string& pem)
{
    const std::string begin = "-----BEGIN CERTIFICATE-----", end = "-----END CERTIFICATE-----";
    const size_t      b = pem.find(begin);
    if (b == std::string::npos) return std::nullopt;
    const size_t e = pem.find(end, b);
    if (e == std::string::npos) return std::nullopt;
    return unbase64(pem.substr(b + begin.size(), e - b - begin.size()));
}

std::string pin_text(const std::array<uint8_t, 32>& h) { return "sha256//" + base64(h.data(), h.size()); }

std::string pin_short(const std::array<uint8_t, 32>& h)
{
    std::string out;
    char        b[4];
    for (int i = 0; i < 8; i++) {
        std::snprintf(b, sizeof b, "%02X", h[i]);
        out += (i ? ":" : "") + std::string(b);
    }
    return out;
}

}   // namespace library
