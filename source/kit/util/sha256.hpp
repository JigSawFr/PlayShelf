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
#include <string>

class Sha256
{
  public:
    Sha256();
    void update(const void* data, size_t size);
    // 64 lower-case hex digits. The object is spent afterwards.
    std::string hex();
    // The 32 bytes. The object is spent afterwards.
    std::array<uint8_t, 32> digest();

  private:
    void block(const uint8_t* p);

    std::array<uint32_t, 8> h;
    uint8_t                 buf[64];
    size_t                  used = 0;
    uint64_t                total = 0;
};

// HMAC-SHA256 of `msg` keyed with `key`.
std::array<uint8_t, 32> hmac_sha256(const void* key, size_t key_size, const void* msg, size_t msg_size);
