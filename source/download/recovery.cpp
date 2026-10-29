// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "download/recovery.hpp"

#include <algorithm>
#include <cctype>

namespace download
{
namespace
{
std::string lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// "HTTP 404" → 404; 0 for none.
int http_status(const std::string& why)
{
    const size_t at = why.find("HTTP ");
    if (at == std::string::npos) return 0;
    int status = 0;
    for (size_t i = at + 5; i < why.size() && i < at + 8 && std::isdigit((unsigned char)why[i]); i++)
        status = status * 10 + (why[i] - '0');
    return status >= 100 && status <= 599 ? status : 0;
}

// "https://host:port/path/" → "https://host:port/path".
std::string trimmed(std::string base)
{
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base;
}

// The scheme, host and port of an address.
std::string origin(const std::string& url)
{
    const size_t scheme = url.find("://");
    if (scheme == std::string::npos) return "";
    const size_t path = url.find('/', scheme + 3);
    return lower(path == std::string::npos ? url : url.substr(0, path));
}
}   // namespace

const char* const SERVER_GONE = "the library server was removed";

Cause cause_of(const std::string& why, bool unsigned_content)
{
    if (unsigned_content) return Cause::Unsigned;
    if (why == SERVER_GONE) return Cause::ServerGone;
    const std::string w = lower(why);
    if (w.contains("not enough free space")) return Cause::NoSpace;
    const int status = http_status(why);
    if (status == 401 || status == 403 || status == 407) return Cause::Auth;
    if (status == 404 || status == 410) return Cause::Gone;
    if (status == 429 || status >= 500) return Cause::Server;
    if (w.contains("pinned public key") || w.contains("certificate") || w.contains("ssl")) return Cause::Certificate;
    if (w.contains("sha-256 differs") || w.contains("came out shorter") || w.contains("does not decompress") ||
        w.contains("more decompressed data") || w.contains("cut short") || w.contains("differs from what the content meta"))
        return Cause::Corrupt;
    static const char* const NETWORK[] = { "resolve", "connect", "timed out", "timeout", "too slow", "reset",
                                           "recv failure", "send failure", "empty reply", "transfer closed",
                                           "connection", "network", "unreachable", "range came out short" };
    for (const char* n : NETWORK)
        if (w.contains(n)) return Cause::Network;
    return Cause::Other;
}

const char* cause_name(Cause c)
{
    switch (c) {
    case Cause::Network: return "network";
    case Cause::Server: return "server";
    case Cause::Auth: return "auth";
    case Cause::Gone: return "gone";
    case Cause::Certificate: return "certificate";
    case Cause::NoSpace: return "no_space";
    case Cause::Corrupt: return "corrupt";
    case Cause::Unsigned: return "unsigned";
    case Cause::ServerGone: return "server_gone";
    case Cause::Other: break;
    }
    return "other";
}

std::optional<int> retry_delay(Cause c, int attempts)
{
    static const int DELAYS[RETRIES] = { 10, 30, 120 };
    if (attempts < 0) attempts = 0;
    if (c != Cause::Network && c != Cause::Server) return std::nullopt;
    return attempts < RETRIES ? std::optional<int>(DELAYS[attempts]) : std::nullopt;
}

std::vector<Remedy> remedies(Cause c, const Situation& s)
{
    std::vector<Remedy> out;
    switch (c) {
    case Cause::Network:
    case Cause::Server:
        out.push_back(Remedy::Retry);
        if (s.server_known && s.other_address) out.push_back(Remedy::OtherAddress);
        break;
    case Cause::Auth:
    case Cause::Certificate:
        if (s.server_known) out.push_back(Remedy::EditServer);
        out.push_back(Remedy::Retry);
        break;
    case Cause::Gone:
        if (s.server_known) out.push_back(Remedy::FindAgain);
        out.push_back(Remedy::Retry);
        break;
    case Cause::Unsigned:
        out.push_back(Remedy::InstallAnyway);
        break;
    case Cause::ServerGone:
        break;
    case Cause::Corrupt:
    case Cause::NoSpace:
    case Cause::Other:
        out.push_back(Remedy::Retry);
        break;
    }
    return out;
}

std::string rebase(const std::string& url, const std::string& from, const std::string& to)
{
    const std::string f = trimmed(from), t = trimmed(to);
    if (f.empty() || t.empty() || origin(f).empty() || origin(t).empty()) return "";
    // Under the address with its path: the rest kept.
    if (lower(url.substr(0, f.size())) == lower(f) && (url.size() == f.size() || url[f.size()] == '/'))
        return t + url.substr(f.size());
    // Else the same host, another path: its origin swapped.
    const std::string o = origin(url);
    if (o.empty() || o != origin(f)) return "";
    return origin(t) + url.substr(o.size());
}

bool installed_already(const Job& job, const std::map<uint64_t, uint32_t>& installed)
{
    if (!job.title_id) return false;
    const auto it = installed.find(job.title_id);
    if (it == installed.end()) return false;
    return !job.version || it->second >= *job.version;
}

long find_again(const Job& job, const std::vector<library::Item>& items)
{
    if (!job.title_id) return -1;
    long by_name = -1;
    for (size_t i = 0; i < items.size(); i++) {
        const library::Item& it = items[i];
        if (it.title_id != job.title_id || it.url.empty()) continue;
        if (job.version && it.has_version && it.version == *job.version) return (long)i;
        if (!job.version && by_name < 0) by_name = (long)i;
    }
    return by_name;
}

}   // namespace download
