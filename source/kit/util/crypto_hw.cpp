// crypto_hw — the engine of util/crypto_backend that runs on the ARMv8
// crypto extensions (AES, SHA-256): the console's CPU has them, a build for
// anything else (a desktop, the host tests) is compiled without them and
// has no engine. Written with the compiler's intrinsics, so the build needs
// -march=armv8-a+crypto (devkitA64's flags carry it); crypto::offer_engine()
// checks it against the portable code before anything relies on it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/crypto_backend.hpp"

#if defined(__aarch64__) && defined(__ARM_FEATURE_AES) && defined(__ARM_FEATURE_SHA2)
#include <arm_neon.h>

namespace crypto
{
namespace
{
// The 11 round keys as vectors: a word of the schedule holds 4 bytes in the order the state has them.
void load_keys(const uint32_t rk[44], uint8x16_t k[11])
{
    for (int r = 0; r < 11; r++) {
        uint8_t b[16];
        for (int c = 0; c < 4; c++) {
            const uint32_t w = rk[4 * r + c];
            b[4 * c] = (uint8_t)(w >> 24);
            b[4 * c + 1] = (uint8_t)(w >> 16);
            b[4 * c + 2] = (uint8_t)(w >> 8);
            b[4 * c + 3] = (uint8_t)w;
        }
        k[r] = vld1q_u8(b);
    }
}

inline uint8x16_t enc_block(uint8x16_t s, const uint8x16_t k[11])
{
    for (int r = 0; r < 9; r++)
        s = vaesmcq_u8(vaeseq_u8(s, k[r]));
    return veorq_u8(vaeseq_u8(s, k[9]), k[10]);
}

// dk[r] is InvMixColumns of round key r: the equivalent inverse cipher.
inline uint8x16_t dec_block(uint8x16_t s, const uint8x16_t k[11], const uint8x16_t dk[11])
{
    s = vaesdq_u8(s, k[10]);
    for (int r = 9; r >= 1; r--)
        s = vaesdq_u8(vaesimcq_u8(s), dk[r]);
    return veorq_u8(s, k[0]);
}

void aes_encrypt(const uint32_t rk[44], const uint8_t* in, uint8_t* out, size_t blocks)
{
    uint8x16_t k[11];
    load_keys(rk, k);
    size_t i = 0;
    // Four blocks at once: they do not depend on each other, so the unit works on all four.
    for (; i + 4 <= blocks; i += 4) {
        uint8x16_t a = vld1q_u8(in + 16 * i), b = vld1q_u8(in + 16 * i + 16), c = vld1q_u8(in + 16 * i + 32), d = vld1q_u8(in + 16 * i + 48);
        for (int r = 0; r < 9; r++) {
            a = vaesmcq_u8(vaeseq_u8(a, k[r]));
            b = vaesmcq_u8(vaeseq_u8(b, k[r]));
            c = vaesmcq_u8(vaeseq_u8(c, k[r]));
            d = vaesmcq_u8(vaeseq_u8(d, k[r]));
        }
        vst1q_u8(out + 16 * i, veorq_u8(vaeseq_u8(a, k[9]), k[10]));
        vst1q_u8(out + 16 * i + 16, veorq_u8(vaeseq_u8(b, k[9]), k[10]));
        vst1q_u8(out + 16 * i + 32, veorq_u8(vaeseq_u8(c, k[9]), k[10]));
        vst1q_u8(out + 16 * i + 48, veorq_u8(vaeseq_u8(d, k[9]), k[10]));
    }
    for (; i < blocks; i++)
        vst1q_u8(out + 16 * i, enc_block(vld1q_u8(in + 16 * i), k));
}

void aes_decrypt(const uint32_t rk[44], const uint8_t* in, uint8_t* out, size_t blocks)
{
    uint8x16_t k[11], dk[11];
    load_keys(rk, k);
    for (int r = 1; r < 10; r++)
        dk[r] = vaesimcq_u8(k[r]);
    for (size_t i = 0; i < blocks; i++)
        vst1q_u8(out + 16 * i, dec_block(vld1q_u8(in + 16 * i), k, dk));
}

const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

void sha256_blocks(uint32_t h[8], const uint8_t* data, size_t blocks)
{
    uint32x4_t s0 = vld1q_u32(h), s1 = vld1q_u32(h + 4);
    for (; blocks > 0; blocks--, data += 64) {
        const uint32x4_t save0 = s0, save1 = s1;
        uint32x4_t       m[4];
        for (int i = 0; i < 4; i++)
            m[i] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data + 16 * i)));
        for (int i = 0; i < 16; i++) {
            // Rounds 4i to 4i+3; from the fifth group on, the schedule is made in the slot it replaces.
            if (i >= 4) m[i % 4] = vsha256su1q_u32(vsha256su0q_u32(m[i % 4], m[(i + 1) % 4]), m[(i + 2) % 4], m[(i + 3) % 4]);
            const uint32x4_t wk = vaddq_u32(m[i % 4], vld1q_u32(K + 4 * i));
            const uint32x4_t before = s0;
            s0 = vsha256hq_u32(s0, s1, wk);
            s1 = vsha256h2q_u32(s1, before, wk);
        }
        s0 = vaddq_u32(s0, save0);
        s1 = vaddq_u32(s1, save1);
    }
    vst1q_u32(h, s0);
    vst1q_u32(h + 4, s1);
}

const Engine ARMV8 = { "armv8-crypto", aes_encrypt, aes_decrypt, sha256_blocks };
}   // namespace

const Engine* hardware_engine() { return &ARMV8; }
}   // namespace crypto

#else

namespace crypto
{
const Engine* hardware_engine() { return nullptr; }
}   // namespace crypto

#endif
