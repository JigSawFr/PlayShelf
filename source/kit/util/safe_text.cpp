// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/safe_text.hpp"

#include <cctype>
#include <cstdint>

#include "util/utf8.hpp"

namespace safe_text
{

namespace
{
bool hidden(uint32_t cp)
{
    return (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2066 && cp <= 0x2069) || cp == 0x200E || cp == 0x200F ||
           cp == 0x061C || cp == 0x200B || cp == 0x200C || cp == 0x2060 || cp == 0xFEFF;
}
}   // namespace

std::string clean(const std::string& text, size_t max_bytes, bool multiline)
{
    std::string out;
    out.reserve(text.size() < max_bytes ? text.size() : max_bytes);
    for (size_t i = 0; i < text.size();) {
        uint32_t cp = 0;
        const size_t n = utf8::decode(text, i, &cp);
        if (!n) {
            i++;
            continue;
        }
        std::string piece;
        if (cp == '\n' || cp == 0x2028 || cp == 0x2029) piece = multiline ? "\n" : " ";
        else if (cp == '\t') piece = " ";
        else if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F) || hidden(cp)) piece = "";
        else piece = text.substr(i, n);
        i += n;
        if (out.size() + piece.size() > max_bytes) break;
        out += piece;
    }
    return out;
}

std::string clean_notes(const std::string& text, size_t max_bytes)
{
    std::string out = clean(text, text.size(), true);
    if (out.size() > max_bytes) {
        const size_t cut = out.rfind('\n', max_bytes);
        out.resize(cut == std::string::npos ? 0 : cut);
    }
    return out;
}

bool is_version(const std::string& text)
{
    if (text.empty() || text.size() > 40 || !std::isalnum((unsigned char)text[0])) return false;
    for (char c : text)
        if (!std::isalnum((unsigned char)c) && c != '.' && c != '+' && c != '-' && c != '_') return false;
    return true;
}

}   // namespace safe_text
