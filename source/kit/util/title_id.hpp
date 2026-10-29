// title_id — a title or application id as the console writes it: 16 hex
// digits, upper case ("0100ABCD12345000"). Header-only, plain C++.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace title_id
{

inline std::string text(uint64_t id)
{
    static constexpr char DIGITS[] = "0123456789ABCDEF";
    std::string           out(16, '0');
    for (int i = 15; i >= 0; i--, id >>= 4) out[i] = DIGITS[id & 0xF];
    return out;
}

}   // namespace title_id
