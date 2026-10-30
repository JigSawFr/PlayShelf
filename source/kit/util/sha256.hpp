// sha256 — SHA-256 (FIPS 180-4), fed in pieces, to check a download against
// the digest GitHub publishes for a release asset, and HMAC-SHA256 (RFC
// 2104), for the desktop simulation of the recovery report's proof
// (core/rescue.h; the Switch uses libnx's). Plain C++ for the host tests
// (tests/dev_builds).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

class Sha256
{
  public:
    Sha256();
    void update(std::span<const uint8_t> bytes);
    void update(std::string_view text);
    // 64 lower-case hex digits. The object is spent afterwards.
    std::string hex();
    // The 32 bytes. The object is spent afterwards.
    std::array<uint8_t, 32> digest();

    // Where it stands, to go on later from there (an install resumed
    // after a pause or a restart): STATE_SIZE bytes. restore() takes them
    // back; false, the object unchanged, for bytes that are not a state.
    static constexpr size_t STATE_SIZE = 32 + 64 + 8 + 8;
    std::array<uint8_t, STATE_SIZE> save() const;
    bool                            restore(std::span<const uint8_t> state);

  private:
    void block(const uint8_t* p);

    std::array<uint32_t, 8> h;
    uint8_t                 buf[64];
    size_t                  used = 0;
    uint64_t                total = 0;
};

// HMAC-SHA256 of `msg` keyed with `key`.
std::array<uint8_t, 32> hmac_sha256(std::span<const uint8_t> key, std::span<const uint8_t> msg);
std::array<uint8_t, 32> hmac_sha256(std::span<const uint8_t> key, std::string_view msg);
