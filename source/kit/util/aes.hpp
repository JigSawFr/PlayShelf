// aes — AES-128 (FIPS 197) and three modes: XTS with Nintendo's tweak (an
// NCA's header), CTR (an NCA's sections, an NCZ's re-encryption, the
// secrets util/secret_box seals) and ECB (an NCA's key area). Plain C++, no
// libnx: the host tests and the fuzzers run it as the console does.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace aes
{

class Aes128
{
  public:
    explicit Aes128(const uint8_t key[16]);
    void encrypt_block(const uint8_t in[16], uint8_t out[16]) const;
    void decrypt_block(const uint8_t in[16], uint8_t out[16]) const;

  private:
    std::array<uint32_t, 44> rk;   // the round keys, 11 of 4 words
};

// ECB: each 16-byte block on its own. data's size is a multiple of 16.
void aes128_ecb_decrypt(const uint8_t key[16], std::span<uint8_t> data);
void aes128_ecb_encrypt(const uint8_t key[16], std::span<uint8_t> data);

// XTS as Nintendo uses it: key is the data key then the tweak key (32
// bytes), the tweak of a sector its number as a big-endian 128-bit integer
// (IEEE 1619 writes it little-endian). data covers whole sectors of
// sector_size bytes (a multiple of 16), the first one numbered `sector`.
void aes128_xts_nintendo(const uint8_t key[32], std::span<uint8_t> data, uint64_t sector, size_t sector_size, bool encrypt);

// CTR with a 16-byte counter block: the block at byte `offset` of the
// stream is encrypted with `iv` plus offset / 16 (a 128-bit big-endian
// addition). crypt() is its own inverse and may start anywhere in a block.
class Aes128Ctr
{
  public:
    Aes128Ctr(const uint8_t key[16], const uint8_t iv[16]);
    void crypt(std::span<uint8_t> data, uint64_t offset) const;

  private:
    Aes128                  aes;
    std::array<uint8_t, 16> iv;
};

}   // namespace aes
