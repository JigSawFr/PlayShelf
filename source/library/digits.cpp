// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/digits.hpp"

namespace library
{

std::string group_digits(uint64_t n, std::string_view sep)
{
    std::string digits = std::to_string(n);
    if (digits.size() < 5) return digits;
    std::string out;
    for (size_t i = 0; i < digits.size(); i++) {
        if (i && (digits.size() - i) % 3 == 0) out += sep;
        out += digits[i];
    }
    return out;
}

}   // namespace library
