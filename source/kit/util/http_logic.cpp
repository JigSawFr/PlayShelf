// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/http_logic.hpp"

#include <cctype>

namespace http_logic
{

bool fits(size_t have, size_t n, size_t max)
{
    return have + n <= max;
}

std::vector<std::string> request_headers(const char* content_type, const std::vector<std::string>* extra)
{
    std::vector<std::string> out;
    if (content_type) {
        out.push_back(std::string("Content-Type: ") + content_type);
        out.push_back("Expect:");
    }
    if (extra) out.insert(out.end(), extra->begin(), extra->end());
    return out;
}

bool succeeded(const Outcome& o, std::string* error)
{
    std::string why;
    if (o.status >= 400) why = "HTTP " + std::to_string(o.status);
    else if (o.refused) why = "unexpected answer (HTTP " + std::to_string(o.status) + ")";
    else if (o.overflow) why = "response too large";
    else if (o.file_error) why = "cannot write the file";
    else if (!o.transfer_ok) why = o.range_error ? RANGE_REFUSED : o.transfer_error;
    else return true;
    if (error) *error = why;
    return false;
}

const char* const RANGE_REFUSED = "the server cannot resume this download";

bool restart_resume(const std::string& error)
{
    return error == "HTTP 416" || error == RANGE_REFUSED;
}

Start download_start(long status, bool resumed)
{
    if (status >= 400) return Start::Refuse;
    if (!resumed) return Start::Write;
    if (status == 206) return Start::Write;
    if (status == 200) return Start::StartOver;
    return Start::Refuse;
}

bool redirect_ok(long status, const std::string& target, std::string* error)
{
    if (status < 300 || status >= 400 || !target.starts_with("https://")) {
        if (error) *error = "no redirect (HTTP " + std::to_string(status) + ")";
        return false;
    }
    return true;
}

bool parse_set_cookie(const std::string& line, std::string* cookie)
{
    const size_t colon = line.find(':');
    if (colon == std::string::npos) return false;
    std::string name = line.substr(0, colon);
    for (char& c : name)
        c = (char)std::tolower((unsigned char)c);
    if (name != "set-cookie") return false;
    std::string v = line.substr(colon + 1, line.find(';', colon) == std::string::npos ? std::string::npos
                                                                                     : line.find(';', colon) - colon - 1);
    auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!v.empty() && space(v.front()))
        v.erase(v.begin());
    while (!v.empty() && space(v.back()))
        v.pop_back();
    const size_t eq = v.find('=');
    if (eq == 0 || eq == std::string::npos) return false;
    for (char c : v)
        if ((unsigned char)c < 32 || c == 127) return false;
    *cookie = v;
    return true;
}

bool parse_content_range(const std::string& line, ContentRange* out)
{
    const size_t colon = line.find(':');
    if (colon == std::string::npos) return false;
    std::string name = line.substr(0, colon);
    for (char& c : name)
        c = (char)std::tolower((unsigned char)c);
    if (name != "content-range") return false;
    size_t i = colon + 1;
    auto skip = [&]() {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            i++;
    };
    auto number = [&](uint64_t* v) {
        const size_t start = i;
        uint64_t     n = 0;
        while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
            const uint64_t d = (uint64_t)(line[i] - '0');
            if (n > (UINT64_MAX - d) / 10) return false;
            n = n * 10 + d;
            i++;
        }
        *v = n;
        return i > start;
    };
    skip();
    if (line.compare(i, 6, "bytes ") != 0) return false;
    i += 6;
    skip();
    auto expect = [&](char c) {
        if (i >= line.size() || line[i] != c) return false;
        i++;
        return true;
    };
    ContentRange r;
    if (!number(&r.first) || !expect('-') || !number(&r.last) || !expect('/') || !number(&r.total)) return false;
    skip();
    while (i < line.size() && (line[i] == '\r' || line[i] == '\n'))
        i++;
    if (i != line.size() || r.first > r.last || r.last >= r.total) return false;
    *out = r;
    return true;
}

bool range_ok(long status, const ContentRange* got, uint64_t first, uint64_t length, std::string* error)
{
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    if (status != 206) return fail("the server does not serve ranges (HTTP " + std::to_string(status) + ")");
    if (!got) return fail("a range answer without its Content-Range");
    if (got->first != first || got->last - got->first + 1 != length) return fail("the server sent another range");
    return true;
}

}   // namespace http_logic
