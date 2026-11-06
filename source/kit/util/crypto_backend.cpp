// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/crypto_backend.hpp"

#include "util/aes.hpp"
#include "util/sha256.hpp"

namespace crypto
{
namespace
{
const Engine* s_offered = nullptr;
bool          s_aes_ok = false;
bool          s_sha_ok = false;
}   // namespace

bool offer_engine(const Engine* engine)
{
    s_offered = engine;
    s_aes_ok = engine && engine->aes_encrypt && engine->aes_decrypt && aes::verify_engine(*engine);
    s_sha_ok = engine && engine->sha256_blocks && Sha256::verify_engine(*engine);
    detail::aes.store(s_aes_ok ? engine : nullptr);
    detail::sha.store(s_sha_ok ? engine : nullptr);
    return engine && (s_aes_ok || !engine->aes_encrypt) && (s_sha_ok || !engine->sha256_blocks);
}

std::string_view backend_name(Backend b)
{
    switch (b) {
        case Backend::Software: return "software";
        case Backend::Hardware: return "hardware";
        default: return "auto";
    }
}

Backend backend_from_name(std::string_view name)
{
    if (name == "software") return Backend::Software;
    if (name == "hardware") return Backend::Hardware;
    return Backend::Auto;
}

std::string status()
{
    std::string out = "crypto ";
    out += backend_name(backend());
    out += ": ";
    if (!s_offered) {
        out += "no hardware engine here, software";
    } else {
        out += "engine ";
        out += s_offered->name;
        out += ", aes ";
        out += !s_offered->aes_encrypt ? "software" : s_aes_ok ? (aes_engine() ? "on it" : "set aside") : "failed its check, software";
        out += ", sha-256 ";
        out += !s_offered->sha256_blocks ? "software" : s_sha_ok ? (sha_engine() ? "on it" : "set aside") : "failed its check, software";
    }
    return out;
}
}   // namespace crypto
