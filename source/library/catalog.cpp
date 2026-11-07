// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/catalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace library
{

std::string display_text(const std::string& s, size_t max)
{
    // Each character decoded: an invalid sequence (a stray continuation
    // byte, an overlong form, a surrogate, past U+10FFFF, cut short) becomes
    // U+FFFD, a control character a space; the text stops before the
    // character that would pass `max` bytes.
    std::string out;
    size_t      i = 0;
    while (i < s.size()) {
        const unsigned char c = (unsigned char)s[i];
        size_t              n = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
        bool                ok = n > 0 && i + n <= s.size();
        uint32_t            cp = n == 1 ? c : n == 2 ? c & 0x1f : n == 3 ? c & 0x0f : c & 0x07;
        for (size_t k = 1; ok && k < n; k++) {
            const unsigned char cc = (unsigned char)s[i + k];
            if ((cc & 0xc0) != 0x80) ok = false;
            cp = cp << 6 | (cc & 0x3f);
        }
        if (ok && ((n == 3 && (cp < 0x800 || (cp >= 0xd800 && cp <= 0xdfff))) || (n == 4 && (cp < 0x10000 || cp > 0x10ffff))))
            ok = false;
        std::string ch;
        if (!ok) {
            ch = "\xef\xbf\xbd";
            n = 1;
        } else if (cp < 32 || cp == 127) {
            ch = " ";
        } else {
            ch = s.substr(i, n);
        }
        if (out.size() + ch.size() > max) break;
        out += ch;
        i += n;
    }
    return out;
}

std::string resolve(const std::string& base, const std::string& ref)
{
    auto https = [](const std::string& u) { return u.size() > 8 && u.starts_with("https://") ? u : std::string(); };
    if (ref.empty()) return "";
    // Any scheme of its own: kept only when https.
    const size_t colon = ref.find(':');
    const size_t slash = ref.find('/');
    if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
        std::string scheme = ref.substr(0, colon);
        std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return scheme == "https" ? https("https" + ref.substr(colon)) : "";
    }
    if (!base.starts_with("https://")) return "";
    const size_t host_end = base.find_first_of("/?#", 8);
    const std::string origin = base.substr(0, host_end);
    if (origin.size() <= 8) return "";
    if (ref.starts_with("//")) return https("https:" + ref);
    std::string path;
    if (ref[0] == '/') {
        path = ref;
    } else {
        std::string base_path = host_end == std::string::npos ? "/" : base.substr(host_end);
        const size_t q = base_path.find_first_of("?#");
        if (q != std::string::npos) base_path.resize(q);
        if (base_path.empty() || base_path[0] != '/') base_path = "/" + base_path;
        path = base_path.substr(0, base_path.find_last_of('/') + 1) + ref;
    }
    // "." and ".." of the path (its query left as it is).
    std::string query;
    const size_t q = path.find_first_of("?#");
    if (q != std::string::npos) {
        query = path.substr(q);
        path.resize(q);
    }
    std::vector<std::string> parts;
    size_t                   at = 1;
    while (at <= path.size()) {
        const size_t next = path.find('/', at);
        const std::string seg = path.substr(at, next == std::string::npos ? std::string::npos : next - at);
        if (seg == "..") {
            if (!parts.empty()) parts.pop_back();
        } else if (seg != ".") {
            parts.push_back(seg);
        }
        if (next == std::string::npos) break;
        at = next + 1;
    }
    std::string out = origin;
    for (const std::string& p : parts)
        out += "/" + p;
    if (parts.empty()) out += "/";
    return https(out + query);
}

Kind kind_of(uint64_t title_id)
{
    if (!title_id) return Kind::Unknown;
    if ((title_id >> 12) & 1) return Kind::Dlc;
    if ((title_id & 0xfff) == 0x800) return Kind::Update;
    if ((title_id & 0xfff) == 0) return Kind::Base;
    return Kind::Unknown;
}

uint64_t base_of(uint64_t title_id)
{
    switch (kind_of(title_id)) {
    case Kind::Update: return title_id & ~0xfffull;
    case Kind::Dlc: return (title_id & ~0xfffull) ^ 0x1000;
    default: return title_id;
    }
}

}   // namespace library
