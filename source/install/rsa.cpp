// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/rsa.hpp"

#include <array>
#include <cstring>

#include "util/sha256.hpp"

namespace install
{
namespace
{
constexpr int LIMBS = 64;   // 2048 bits in 32-bit words, least significant first
using Big = std::array<uint32_t, LIMBS>;

Big from_bytes(const uint8_t b[256])
{
    Big x{};
    for (int i = 0; i < LIMBS; i++) {
        const uint8_t* p = b + 256 - 4 * (i + 1);
        x[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    }
    return x;
}

void to_bytes(const Big& x, uint8_t b[256])
{
    for (int i = 0; i < LIMBS; i++) {
        uint8_t* p = b + 256 - 4 * (i + 1);
        p[0] = (uint8_t)(x[i] >> 24);
        p[1] = (uint8_t)(x[i] >> 16);
        p[2] = (uint8_t)(x[i] >> 8);
        p[3] = (uint8_t)x[i];
    }
}

// a < b
bool less(const Big& a, const Big& b)
{
    for (int i = LIMBS - 1; i >= 0; i--)
        if (a[i] != b[i]) return a[i] < b[i];
    return false;
}

// a -= b, returning the borrow.
uint32_t sub(Big& a, const Big& b)
{
    uint64_t borrow = 0;
    for (int i = 0; i < LIMBS; i++) {
        const uint64_t d = (uint64_t)a[i] - b[i] - borrow;
        a[i] = (uint32_t)d;
        borrow = (d >> 32) & 1;
    }
    return (uint32_t)borrow;
}

// Montgomery multiplication: a * b / 2^2048 mod n (CIOS), with
// n0 = -n^-1 mod 2^32. a and b below n; so is the result.
Big mont_mul(const Big& a, const Big& b, const Big& n, uint32_t n0)
{
    std::array<uint32_t, LIMBS + 2> t{};
    for (int i = 0; i < LIMBS; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < LIMBS; j++) {
            const uint64_t s = (uint64_t)t[j] + (uint64_t)a[j] * b[i] + carry;
            t[j] = (uint32_t)s;
            carry = s >> 32;
        }
        uint64_t s = (uint64_t)t[LIMBS] + carry;
        t[LIMBS] = (uint32_t)s;
        t[LIMBS + 1] = (uint32_t)(s >> 32);
        const uint32_t m = t[0] * n0;
        carry = ((uint64_t)t[0] + (uint64_t)m * n[0]) >> 32;
        for (int j = 1; j < LIMBS; j++) {
            s = (uint64_t)t[j] + (uint64_t)m * n[j] + carry;
            t[j - 1] = (uint32_t)s;
            carry = s >> 32;
        }
        s = (uint64_t)t[LIMBS] + carry;
        t[LIMBS - 1] = (uint32_t)s;
        t[LIMBS] = t[LIMBS + 1] + (uint32_t)(s >> 32);
    }
    Big r;
    for (int i = 0; i < LIMBS; i++)
        r[i] = t[i];
    if (t[LIMBS] || !less(r, n)) sub(r, n);
    return r;
}

// 2^4096 mod n, for into-Montgomery conversions: 1 doubled 4096 times.
Big r_squared(const Big& n)
{
    Big x{};
    x[0] = 1;
    for (int k = 0; k < 4096; k++) {
        uint32_t carry = 0;
        for (int i = 0; i < LIMBS; i++) {
            const uint32_t next = x[i] >> 31;
            x[i] = (x[i] << 1) | carry;
            carry = next;
        }
        if (carry || !less(x, n)) sub(x, n);
    }
    return x;
}

void mgf1_xor(uint8_t* out, size_t size, const uint8_t seed[32])
{
    for (uint32_t counter = 0, done = 0; done < size; counter++) {
        Sha256 h;
        h.update({seed, 32});
        const uint8_t c[4] = {(uint8_t)(counter >> 24), (uint8_t)(counter >> 16), (uint8_t)(counter >> 8), (uint8_t)counter};
        h.update(c);
        const auto mask = h.digest();
        for (size_t i = 0; i < 32 && done < size; i++)
            out[done++] ^= mask[i];
    }
}
}   // namespace

bool rsa2048_pss_sha256_verify(std::span<const uint8_t> msg, const uint8_t signature[256], const uint8_t modulus[256])
{
    const Big n = from_bytes(modulus);
    const Big s = from_bytes(signature);
    if ((n[0] & 1) == 0 || (n[LIMBS - 1] >> 31) == 0) return false;   // an odd 2048-bit modulus only
    if (!less(s, n)) return false;

    // n0 = -n^-1 mod 2^32 (Newton: each step doubles the bits right).
    uint32_t inv = 1;
    for (int i = 0; i < 5; i++)
        inv *= 2 - n[0] * inv;
    const uint32_t n0 = (uint32_t)0 - inv;

    // s^65537 mod n: into Montgomery form, 16 squarings, one multiplication, out.
    const Big r2 = r_squared(n);
    const Big sm = mont_mul(s, r2, n, n0);
    Big       x = sm;
    for (int i = 0; i < 16; i++)
        x = mont_mul(x, x, n, n0);
    x = mont_mul(x, sm, n, n0);
    Big one{};
    one[0] = 1;
    x = mont_mul(x, one, n, n0);

    // EMSA-PSS: emBits 2047, so the 256 bytes' top bit is zero.
    uint8_t em[256];
    to_bytes(x, em);
    if (em[255] != 0xbc || (em[0] & 0x80)) return false;
    constexpr size_t H = 32, SALT = 32, DB = 256 - H - 1;
    uint8_t          hash[H];
    std::memcpy(hash, em + DB, H);
    mgf1_xor(em, DB, hash);
    em[0] &= 0x7f;
    for (size_t i = 0; i < DB - SALT - 1; i++)
        if (em[i] != 0) return false;
    if (em[DB - SALT - 1] != 0x01) return false;

    Sha256 m;
    m.update(msg);
    const auto mhash = m.digest();
    Sha256     check;
    const uint8_t zeros[8] = {};
    check.update(zeros);
    check.update(mhash);
    check.update({em + DB - SALT, SALT});
    const auto expect = check.digest();
    return std::memcmp(expect.data(), hash, H) == 0;
}

}   // namespace install
