// bytes — little-endian reads at an offset, for the parsers of install/:
// the caller has checked the bounds.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <utility>

namespace install
{

// What a parser gives: the value, else why it refused the bytes (in English).
template <class T> using Parsed = std::expected<T, std::string>;
inline std::unexpected<std::string> fail(std::string why) { return std::unexpected(std::move(why)); }
// What a step gives: done, or why not.
using Done = std::expected<void, std::string>;

inline uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
inline uint64_t le64(const uint8_t* p) { return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32; }

inline void put_le16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
inline void put_le32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
inline void put_le64(uint8_t* p, uint64_t v)
{
    for (int i = 0; i < 8; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

// a + b without wrapping: false when it would.
inline bool add_ok(uint64_t a, uint64_t b, uint64_t* out)
{
    *out = a + b;
    return *out >= a;
}

}   // namespace install
