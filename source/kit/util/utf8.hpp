// utf8 — reading and writing one UTF-8 character. Two readers, as their
// callers need: decode() validates (what comes from outside: safe_text,
// the exports), decode_loose() trusts the text (already cleaned, or shipped
// with the app) and never fails; and where to cut a text between two
// characters. Header-only, plain C++.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace utf8
{

// The code point of the well-formed UTF-8 sequence at `s[i]` and its length;
// 0 when there is none there (a stray continuation byte, an overlong form, a
// surrogate, beyond U+10FFFF, cut short).
inline size_t decode(const std::string& s, size_t i, uint32_t* cp)
{
    const unsigned char c = (unsigned char)s[i];
    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    size_t n;
    uint32_t v, min;
    if ((c & 0xE0) == 0xC0) n = 2, v = c & 0x1F, min = 0x80;
    else if ((c & 0xF0) == 0xE0) n = 3, v = c & 0x0F, min = 0x800;
    else if ((c & 0xF8) == 0xF0) n = 4, v = c & 0x07, min = 0x10000;
    else return 0;
    if (i + n > s.size()) return 0;
    for (size_t k = 1; k < n; k++) {
        const unsigned char d = (unsigned char)s[i + k];
        if ((d & 0xC0) != 0x80) return 0;
        v = (v << 6) | (d & 0x3F);
    }
    if (v < min || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) return 0;
    *cp = v;
    return n;
}

// decode(), with a byte that starts no well-formed character read as U+FFFD,
// length 1: what an export writes in its place.
inline size_t decode_replacing(const std::string& s, size_t i, uint32_t* cp)
{
    const size_t n = decode(s, i, cp);
    if (n) return n;
    *cp = 0xFFFD;
    return 1;
}

// The sequence at `s[i]` read without checking it: the length its first
// byte announces (1 for a byte that starts none, which is its own code
// point) and the bits that are there. Never 0. For text known to be valid.
inline size_t decode_loose(const std::string& s, size_t i, uint32_t* cp)
{
    const unsigned char c = (unsigned char)s[i];
    const size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
    *cp = n == 1 ? c : n == 2 ? c & 0x1F : n == 3 ? c & 0x0F : c & 0x07;
    for (size_t k = 1; k < n && i + k < s.size(); k++) *cp = (*cp << 6) | ((unsigned char)s[i + k] & 0x3F);
    return n;
}

// Where to cut `s` to at most `n` bytes without splitting a character: `n`,
// moved back over the continuation bytes (10xxxxxx) found there. `n` itself
// at the end of `s` or past it.
inline size_t boundary_before(const std::string& s, size_t n)
{
    while (n > 0 && n < s.size() && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    return n;
}

// `cp` (at most U+10FFFF) as UTF-8 at the end of `out`.
inline void append(std::string& out, uint32_t cp)
{
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

}   // namespace utf8
