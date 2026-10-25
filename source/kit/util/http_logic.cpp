// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/http_logic.hpp"

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
    if (status < 300 || status >= 400 || target.compare(0, 8, "https://") != 0) {
        if (error) *error = "no redirect (HTTP " + std::to_string(status) + ")";
        return false;
    }
    return true;
}

}   // namespace http_logic
