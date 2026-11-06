// util/crypto_backend: the engine the AES modes and SHA-256 run on when the
// platform offers one. A model engine (the portable code behind the engine's
// interface, counting its calls) stands for the console's CPU: with it, every
// mode gives the same bytes as without, over sizes that cross the batches and
// offsets that start inside a block; Software never calls it; an engine that
// answers wrongly is refused at the offer and the results stay right. On a
// CPU that has the real one (aarch64 with the crypto extensions) it is
// checked the same way.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "util/aes.hpp"
#include "util/crypto_backend.hpp"
#include "util/crypto_bench.hpp"
#include "util/sha256.hpp"

namespace
{
int calls_aes = 0, calls_sha = 0;

void model_encrypt(const uint32_t rk[44], const uint8_t* in, uint8_t* out, size_t blocks)
{
    calls_aes++;
    // The portable block cipher from a key schedule: the first four words are the key.
    uint8_t key[16];
    for (int i = 0; i < 4; i++)
        for (int b = 0; b < 4; b++)
            key[4 * i + b] = (uint8_t)(rk[i] >> (24 - 8 * b));
    const aes::Aes128 a(key);
    for (size_t i = 0; i < blocks; i++)
        a.encrypt_block(in + 16 * i, out + 16 * i);
}

void model_decrypt(const uint32_t rk[44], const uint8_t* in, uint8_t* out, size_t blocks)
{
    calls_aes++;
    uint8_t key[16];
    for (int i = 0; i < 4; i++)
        for (int b = 0; b < 4; b++)
            key[4 * i + b] = (uint8_t)(rk[i] >> (24 - 8 * b));
    const aes::Aes128 a(key);
    for (size_t i = 0; i < blocks; i++)
        a.decrypt_block(in + 16 * i, out + 16 * i);
}

uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

// SHA-256's compression, written again here so the model does not lean on the code it checks.
void model_sha(uint32_t h[8], const uint8_t* data, size_t blocks)
{
    calls_sha++;
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
        0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
        0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
        0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
        0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    for (size_t n = 0; n < blocks; n++) {
        const uint8_t* p = data + 64 * n;
        uint32_t       w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
        for (int i = 16; i < 64; i++)
            w[i] = w[i - 16] + (rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7] +
                   (rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10));
        uint32_t v[8];
        std::memcpy(v, h, sizeof(v));
        for (int i = 0; i < 64; i++) {
            const uint32_t t1 = v[7] + (rotr(v[4], 6) ^ rotr(v[4], 11) ^ rotr(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + K[i] + w[i];
            const uint32_t t2 = (rotr(v[0], 2) ^ rotr(v[0], 13) ^ rotr(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
            std::memmove(v + 1, v, 7 * sizeof(uint32_t));
            v[4] += t1;
            v[0] = t1 + t2;
        }
        for (int i = 0; i < 8; i++)
            h[i] += v[i];
    }
}

const crypto::Engine MODEL = { "model", model_encrypt, model_decrypt, model_sha };

// An engine whose AES drops a bit in the last block of a long run, and whose SHA-256 is right.
void bad_encrypt(const uint32_t rk[44], const uint8_t* in, uint8_t* out, size_t blocks)
{
    model_encrypt(rk, in, out, blocks);
    if (blocks > 3) out[16 * blocks - 1] ^= 1;
}
const crypto::Engine BAD_AES = { "bad-aes", bad_encrypt, model_decrypt, model_sha };

void bad_sha(uint32_t h[8], const uint8_t* data, size_t blocks)
{
    model_sha(h, data, blocks);
    h[7] ^= 0x80000000u;
}
const crypto::Engine BAD_SHA = { "bad-sha", model_encrypt, model_decrypt, bad_sha };

std::vector<uint8_t> pattern(size_t n, uint32_t seed)
{
    std::vector<uint8_t> v(n);
    for (uint8_t& b : v) {
        seed = seed * 1664525u + 1013904223u;
        b = (uint8_t)(seed >> 24);
    }
    return v;
}

// Everything the modes do, in one byte string, to compare runs.
std::vector<uint8_t> everything()
{
    std::vector<uint8_t> out;
    const std::vector<uint8_t> key = pattern(32, 1), iv = pattern(16, 2);
    // CTR: sizes around the batch (32 blocks = 512 bytes), starting inside a block.
    for (size_t size : std::initializer_list<size_t>{ 0, 1, 15, 16, 17, 511, 512, 513, 1030, 4099 })
        for (uint64_t offset : std::initializer_list<uint64_t>{ 0, 1, 15, 16, 17, 0x1ff, 0x1000f, 0xfffffffffffffff0ull }) {
            std::vector<uint8_t> d = pattern(size, (uint32_t)(size + offset));
            aes::Aes128Ctr(key.data(), iv.data()).crypt(d, offset);
            out.insert(out.end(), d.begin(), d.end());
        }
    // XTS: whole sectors of 0x200, of 0x10 and of 0x420 (more than a batch), both ways.
    for (size_t sector_size : std::initializer_list<size_t>{ 0x10, 0x200, 0x420 })
        for (bool encrypt : { true, false }) {
            std::vector<uint8_t> d = pattern(sector_size * 3, (uint32_t)sector_size);
            aes::aes128_xts_nintendo(key.data(), d, 7, sector_size, encrypt);
            out.insert(out.end(), d.begin(), d.end());
        }
    // ECB, both ways.
    std::vector<uint8_t> e = pattern(16 * 70, 5);
    aes::aes128_ecb_encrypt(key.data(), e);
    out.insert(out.end(), e.begin(), e.end());
    aes::aes128_ecb_decrypt(key.data(), e);
    out.insert(out.end(), e.begin(), e.end());
    // SHA-256: lengths around the block, in one piece and in pieces, with a save and a restore between.
    for (size_t size : std::initializer_list<size_t>{ 0, 1, 55, 56, 63, 64, 65, 127, 128, 129, 1000, 5000 }) {
        const std::vector<uint8_t> d = pattern(size, (uint32_t)size);
        Sha256                     whole;
        whole.update(d);
        const auto a = whole.digest();
        out.insert(out.end(), a.begin(), a.end());
        Sha256 parts;
        size_t cut = size / 3;
        parts.update({ d.data(), cut });
        Sha256 resumed;
        resumed.restore(parts.save());
        resumed.update({ d.data() + cut, size - cut });
        const auto b = resumed.digest();
        out.insert(out.end(), b.begin(), b.end());
    }
    return out;
}

void reset() { crypto::set_backend(crypto::Backend::Auto); crypto::offer_engine(nullptr); calls_aes = calls_sha = 0; }
}   // namespace

int main()
{
    // Without an engine: the portable code, said so.
    reset();
    CHECK(crypto::aes_engine() == nullptr && crypto::sha_engine() == nullptr);
    CHECK(crypto::status().find("no hardware engine") != std::string::npos);
    const std::vector<uint8_t> want = everything();

    // The known answers, whatever runs: FIPS 197 C.1 and SHA-256 "abc".
    {
        uint8_t key[16], block[16];
        for (int i = 0; i < 16; i++) {
            key[i] = (uint8_t)i;
            block[i] = (uint8_t)(i * 0x11);
        }
        aes::aes128_ecb_encrypt(key, block);
        const uint8_t fips[16] = { 0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30, 0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a };
        CHECK(std::memcmp(block, fips, 16) == 0);
        Sha256 s;
        s.update(std::string_view("abc"));
        CHECK(s.hex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    }

    // A correct engine passes its offer, is used, and changes no byte.
    CHECK(crypto::offer_engine(&MODEL));
    CHECK(crypto::aes_engine() == &MODEL && crypto::sha_engine() == &MODEL);
    CHECK(crypto::status().find("model") != std::string::npos);
    calls_aes = calls_sha = 0;
    CHECK(everything() == want);
    CHECK(calls_aes > 0 && calls_sha > 0);

    // Hardware asked for: the same engine. Software: never called, same bytes.
    crypto::set_backend(crypto::Backend::Hardware);
    CHECK(crypto::aes_engine() == &MODEL);
    crypto::set_backend(crypto::Backend::Software);
    CHECK(crypto::aes_engine() == nullptr && crypto::sha_engine() == nullptr);
    calls_aes = calls_sha = 0;
    CHECK(everything() == want);
    CHECK(calls_aes == 0 && calls_sha == 0);
    crypto::set_backend(crypto::Backend::Auto);

    // An engine that answers wrongly is not used, and the bytes stay right.
    CHECK(!crypto::offer_engine(&BAD_AES));
    CHECK(crypto::aes_engine() == nullptr);
    CHECK(crypto::status().find("failed its check") != std::string::npos);
    calls_aes = calls_sha = 0;
    CHECK(everything() == want);
    CHECK(calls_aes == 0);   // nothing reached the bad AES
    // Its SHA-256 was right: the primitives are checked one by one.
    CHECK(crypto::sha_engine() == &BAD_AES);
    CHECK(!crypto::offer_engine(&BAD_SHA));
    CHECK(crypto::sha_engine() == nullptr);
    CHECK(everything() == want);

    // Offering nothing puts the portable code back.
    CHECK(crypto::offer_engine(&MODEL));
    CHECK(!crypto::offer_engine(nullptr));
    CHECK(crypto::aes_engine() == nullptr && crypto::sha_engine() == nullptr);

    // The setting's names.
    CHECK(crypto::backend_from_name("software") == crypto::Backend::Software);
    CHECK(crypto::backend_from_name("hardware") == crypto::Backend::Hardware);
    CHECK(crypto::backend_from_name("auto") == crypto::Backend::Auto);
    CHECK(crypto::backend_from_name("") == crypto::Backend::Auto);
    CHECK(crypto::backend_from_name("quantum") == crypto::Backend::Auto);
    CHECK(crypto::backend_name(crypto::Backend::Hardware) == "hardware");

    // The benchmark: three rows; an engine gets a second figure; the setting is put back.
    reset();
    {
        const auto rows = crypto::benchmark(std::chrono::milliseconds(5));
        CHECK(rows.size() == 3 && rows[0].software > 0 && rows[0].engine < 0);
        CHECK(crypto::bench_text(rows).find("no hardware") != std::string::npos);
        CHECK(crypto::offer_engine(&MODEL));
        crypto::set_backend(crypto::Backend::Hardware);
        const auto with = crypto::benchmark(std::chrono::milliseconds(5));
        CHECK(with.size() == 3 && with[1].engine > 0 && with[2].engine > 0);
        CHECK(crypto::backend() == crypto::Backend::Hardware);
        CHECK(crypto::bench_text(with).find("AES-XTS") != std::string::npos);
    }

    // The CPU's own engine, where there is one: it passes the check and gives the same bytes.
    reset();
    if (const crypto::Engine* hw = crypto::hardware_engine()) {
        CHECK(crypto::offer_engine(hw));
        CHECK(everything() == want);
    } else {
        CHECK(!crypto::offer_engine(crypto::hardware_engine()));
    }
    reset();
    CHECK_DONE("crypto");
}
