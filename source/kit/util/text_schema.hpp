// text_schema — the "schema=N" first line of a plain-text list kept on the
// SD card (one entry per line: the wishlist, the games put away, the
// notices read). A file without it is schema 1, written before the line
// was; an older build skips the line, which is none of its entries. A
// reader treats a newer schema's file as empty and never saves over it.
// Header-only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "util/number.hpp"

namespace text_schema
{

// The schema of `text`: N of a first line "schema=N", else 1; INT32_MAX
// when N is not a number (a newer build's, as far as this one knows).
inline int of(std::string_view text)
{
    constexpr std::string_view KEY = "schema=";
    if (!text.starts_with(KEY)) return 1;
    std::string_view line = text.substr(KEY.size(), text.find('\n') - KEY.size());
    if (line.ends_with('\r')) line.remove_suffix(1);
    const auto n = number::parse<int32_t>(line);
    return n && *n > 0 ? *n : INT32_MAX;
}

// `body` (its lines) with the "schema=N" line on top.
inline std::string with(int schema, const std::string& body)
{
    return "schema=" + std::to_string(schema) + "\n" + body;
}

}   // namespace text_schema
