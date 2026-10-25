// emoji — telling the emoji in a text: the console's shared font draws none
// of them, so the change-log and the Markdown views show a picture for the
// one a line opens on (resources/img/emoji/<hex>.png) and drop the others.
// Header-only, plain C++.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "util/utf8.hpp"

namespace emoji
{

// The joiner, the variation selectors and the keycap mark: they only go with
// the emoji before them.
inline bool is_modifier(uint32_t cp)
{
    return cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F) || cp == 0x20E3;
}

// Pictographs, dingbats and the modifiers that go with them.
inline bool is_emoji(uint32_t cp)
{
    return cp >= 0x1F000 || (cp >= 0x2600 && cp <= 0x27BF) || (cp >= 0x2B00 && cp <= 0x2BFF) ||
           (cp >= 0x231A && cp <= 0x231B) || (cp >= 0x23E9 && cp <= 0x23FA) ||   // ⌚ ⏱ ⏳ …
           is_modifier(cp);
}

// The emoji at `s[at]`, as its code point in hex: "1f680" for "🚀 Features".
// Empty when there is none there (a modifier alone is none).
inline std::string icon_at(const std::string& s, size_t at = 0)
{
    if (at >= s.size()) return "";
    uint32_t cp = 0;
    utf8::decode_loose(s, at, &cp);
    if (!is_emoji(cp) || is_modifier(cp)) return "";
    char hex[9];
    std::snprintf(hex, sizeof hex, "%x", (unsigned)cp);
    return hex;
}

// `s` without its emoji (valid UTF-8 expected: see utf8::decode_loose).
inline std::string drop(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = 0;
        const size_t n = utf8::decode_loose(s, i, &cp);
        if (!is_emoji(cp)) out.append(s, i, n);
        i += n;
    }
    return out;
}

}   // namespace emoji
