// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/secret_box.hpp"

#include <cstring>
#include <random>
#include <vector>

#include "app_id.h"
#include "core/device_key.h"
#include "util/aes.hpp"
#include "util/sha256.hpp"

namespace secret_box
{
namespace
{
const char PREFIX[] = "v1:";

// Each key the HMAC of the console's with the app and its purpose
// ("playshelf secret box: encrypt"): one to encrypt, one to authenticate.
void derive(const Key& key, Key* enc, std::array<uint8_t, 32>* mac)
{
    const auto e = hmac_sha256(key, APP_ID " secret box: encrypt");
    std::memcpy(enc->data(), e.data(), 16);
    *mac = hmac_sha256(key, APP_ID " secret box: authenticate");
}

std::string b64(const std::vector<uint8_t>& d)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string       out;
    for (size_t i = 0; i < d.size(); i += 3) {
        const uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < d.size() ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < d.size() ? d[i + 2] : 0);
        out += T[v >> 18 & 63];
        out += T[v >> 12 & 63];
        out += i + 1 < d.size() ? T[v >> 6 & 63] : '=';
        out += i + 2 < d.size() ? T[v & 63] : '=';
    }
    return out;
}

std::optional<std::vector<uint8_t>> unb64(const std::string& s)
{
    std::vector<uint8_t> out;
    uint32_t             acc = 0;
    int                  bits = 0;
    size_t               pad = 0;
    for (char c : s) {
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
        } else
            return std::nullopt;
        if (pad) return std::nullopt;
        acc = acc << 6 | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(acc >> bits));
        }
    }
    // The bits left over pad the last byte: zeros, so that one box has one
    // spelling and a changed character never opens as the same bytes.
    if (acc & ((1u << bits) - 1)) return std::nullopt;
    return out;
}

std::array<uint8_t, 32> tag_of(const std::array<uint8_t, 32>& mac, const uint8_t* nonce, const uint8_t* data, size_t size)
{
    std::vector<uint8_t> m(PREFIX, PREFIX + 2);
    m.insert(m.end(), nonce, nonce + 16);
    m.insert(m.end(), data, data + size);
    return hmac_sha256(mac, m);
}

std::optional<Key> console_key()
{
    Key k;
    if (device_key(k.data()) != 0) return std::nullopt;
    return k;
}
}   // namespace

bool is_sealed(const std::string& text) { return text.compare(0, 3, PREFIX) == 0; }

std::string seal(const std::string& secret, const Key& key, const std::array<uint8_t, 16>& nonce)
{
    Key                     enc;
    std::array<uint8_t, 32> mac;
    derive(key, &enc, &mac);
    std::vector<uint8_t> body(nonce.begin(), nonce.end());
    body.insert(body.end(), secret.begin(), secret.end());
    aes::Aes128Ctr(enc.data(), nonce.data()).crypt(std::span(body).subspan(16), 0);
    const auto tag = tag_of(mac, nonce.data(), body.data() + 16, secret.size());
    body.insert(body.end(), tag.begin(), tag.end());
    return PREFIX + b64(body);
}

std::optional<std::string> open(const std::string& box, const Key& key)
{
    if (!is_sealed(box)) return std::nullopt;
    const auto body = unb64(box.substr(3));
    if (!body || body->size() < 16 + 32) return std::nullopt;
    Key                     enc;
    std::array<uint8_t, 32> mac;
    derive(key, &enc, &mac);
    const size_t n = body->size() - 48;
    const auto   tag = tag_of(mac, body->data(), body->data() + 16, n);
    // Compared in constant time: how much of the tag matched says nothing.
    uint8_t diff = 0;
    for (size_t i = 0; i < 32; i++)
        diff |= (uint8_t)(tag[i] ^ (*body)[16 + n + i]);
    if (diff) return std::nullopt;
    std::string out(body->begin() + 16, body->begin() + 16 + (long)n);
    aes::Aes128Ctr(enc.data(), body->data()).crypt({(uint8_t*)out.data(), out.size()}, 0);
    return out;
}

std::optional<std::string> seal(const std::string& secret)
{
    const auto key = console_key();
    if (!key) return std::nullopt;
    std::array<uint8_t, 16> nonce;
    std::random_device      rd;
    for (auto& b : nonce)
        b = (uint8_t)rd();
    return seal(secret, *key, nonce);
}

std::optional<std::string> open(const std::string& box)
{
    const auto key = console_key();
    if (!key) return std::nullopt;
    return open(box, *key);
}

}   // namespace secret_box
