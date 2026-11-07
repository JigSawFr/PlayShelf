// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/server.hpp"

#include <cctype>

namespace library
{

std::string choose_endpoint(const Endpoints& e, const std::function<bool(const std::string&)>& answers)
{
    if (e.lan_url.empty()) return e.wan_url;
    if (e.wan_url.empty()) return e.lan_url;
    switch (e.connection) {
    case Connection::Lan: return e.lan_url;
    case Connection::Wan: return e.wan_url;
    case Connection::Auto: return answers && answers(e.lan_url) ? e.lan_url : e.wan_url;
    }
    return e.wan_url;
}


bool same_origin(const std::string& a, const std::string& b)
{
    auto origin = [](const std::string& url) {
        const size_t scheme = url.find("://");
        if (scheme == std::string::npos) return std::string();
        const size_t end = url.find_first_of("/?#", scheme + 3);
        std::string  o = url.substr(0, end);
        for (char& ch : o) ch = (char)std::tolower((unsigned char)ch);
        // Credentials in the address ("user@host") are not part of it.
        const size_t at = o.find('@', scheme + 3);
        if (at != std::string::npos) o.erase(scheme + 3, at - scheme - 2);
        if (o.size() > 4 && o.compare(o.size() - 4, 4, ":443") == 0 && o.starts_with("https://")) o.resize(o.size() - 4);
        return o;
    };
    const std::string oa = origin(a);
    return !oa.empty() && oa == origin(b);
}


}   // namespace library
