// crypto_backend — which implementation the AES (util/aes) and SHA-256
// (util/sha256) modes run on: the portable one written here, or an engine
// the platform offers (the ARMv8 crypto extensions of the console's CPU,
// util/crypto_hw). The portable one is always there; an engine is used only
// after it answered known vectors the way the portable one does, so a
// broken engine falls back to the portable one instead of corrupting an
// install. The choice is a setting (Backend): Auto takes the engine when
// there is one, Software never does, Hardware asks for it and says in
// status() when it was not there.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace crypto
{
enum class Backend
{
    Auto,
    Software,
    Hardware
};

// What an engine does. Each function may be null (that primitive stays
// portable). `in` and `out` may be the same bytes.
struct Engine
{
    const char* name;
    // AES-128 on `blocks` blocks of 16 bytes; rk is the key schedule, 44
    // words in FIPS 197 order (Aes128::round_keys()).
    void (*aes_encrypt)(const uint32_t rk[44], const uint8_t* in, uint8_t* out, size_t blocks);
    void (*aes_decrypt)(const uint32_t rk[44], const uint8_t* in, uint8_t* out, size_t blocks);
    // SHA-256's compression function on `blocks` blocks of 64 bytes.
    void (*sha256_blocks)(uint32_t h[8], const uint8_t* data, size_t blocks);
};

namespace detail
{
inline std::atomic<int>           mode{ 0 };   // a Backend
inline std::atomic<const Engine*> aes{ nullptr };   // verified engines
inline std::atomic<const Engine*> sha{ nullptr };
}   // namespace detail

inline void    set_backend(Backend b) { detail::mode.store((int)b); }
inline Backend backend() { return (Backend)detail::mode.load(); }

// The engine the AES modes use now, or null for the portable code.
inline const Engine* aes_engine() { return backend() == Backend::Software ? nullptr : detail::aes.load(); }
// The same for SHA-256.
inline const Engine* sha_engine() { return backend() == Backend::Software ? nullptr : detail::sha.load(); }

// Offers an engine (null: none). It is checked against the portable code,
// primitive by primitive; what fails is left out. Returns true when every
// primitive it has passed.
bool offer_engine(const Engine* engine);

// The engine the platform has, or null: the CPU's crypto extensions on the
// console, nothing on a desktop or in the host tests.
const Engine* hardware_engine();

// One line for the log and the diagnostics: the setting, and what runs.
std::string status();
// "auto", "software", "hardware" and back; anything else is Auto.
std::string_view backend_name(Backend b);
Backend          backend_from_name(std::string_view name);
}   // namespace crypto
