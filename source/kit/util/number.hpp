// number — a whole text read as a number, with std::from_chars: digits only
// (no sign, space or "0x"), nothing left over, no overflow. Header-only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <charconv>
#include <concepts>
#include <optional>
#include <string_view>
#include <system_error>

namespace number
{

// `text` in `base` (10 or 16), or nothing when any of it is not a digit, it
// is empty, or the value does not fit in T.
template <std::integral T>
std::optional<T> parse(std::string_view text, int base = 10)
{
    if (text.empty() || text[0] == '-') return std::nullopt;   // from_chars takes a minus for a signed T
    T          value{};
    const auto end = text.data() + text.size();
    const auto [at, ec] = std::from_chars(text.data(), end, value, base);
    if (ec != std::errc{} || at != end) return std::nullopt;
    return value;
}

}   // namespace number
