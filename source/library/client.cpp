// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/client.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <set>

namespace library
{
namespace
{
std::string without_slash(std::string url)
{
    while (url.size() > 8 && url.back() == '/')
        url.pop_back();
    return url;
}

Fetch failed(const std::string& why)
{
    Fetch f;
    f.status = FetchStatus::Failed;
    f.why = why;
    return f;
}

// What an answer that is not a 200 says.
std::string refusal(const Answer& a)
{
    if (a.status == 0) return a.error.empty() ? "the server does not answer" : a.error;
    if (a.status == 401 || a.status == 403) return "the server refused the username or password (HTTP " + std::to_string(a.status) + ")";
    return "the server answered HTTP " + std::to_string(a.status);
}

// The JSON index at `url` and the ones its "directories" name, breadth
// first, MAX_INDEX_DEPTH levels and MAX_INDEX_FETCHES answers at most,
// merged: an item once, the first message of the day.
bool read_indexes(const std::string& url, const std::vector<std::string>& headers, const std::string& pin, Transport& t,
                  const Answer* first, Catalog* out, std::string* why)
{
    std::deque<std::pair<std::string, int>> todo{{url, 0}};
    std::set<std::string>                   asked, items;
    size_t                                  fetched = 0;
    while (!todo.empty() && fetched < MAX_INDEX_FETCHES) {
        const auto [at, depth] = todo.front();
        todo.pop_front();
        if (!asked.insert(at).second) continue;
        const Answer a = fetched == 0 && first ? *first : t.get(at, headers, pin);
        fetched++;
        if (a.status != 200) {
            if (fetched == 1) {
                *why = refusal(a);
                return false;
            }
            out->skipped++;   // a sub-index that cannot be read: its items are missing
            continue;
        }
        std::string err;
        const auto  c = parse_index(a.body, at, &err);
        if (!c) {
            if (fetched == 1) {
                *why = err;
                return false;
            }
            out->skipped++;
            continue;
        }
        if (out->motd.empty()) out->motd = c->motd;
        for (const Item& i : c->items)
            if (out->items.size() < MAX_ITEMS && items.insert(i.url).second) out->items.push_back(i);
        for (const auto& kv : c->titles)
            out->titles.insert(kv);
        out->skipped += c->skipped;
        if (depth + 1 <= MAX_INDEX_DEPTH)
            for (const std::string& d : c->directories)
                todo.emplace_back(d, depth + 1);
    }
    return true;
}
}   // namespace

Answer Transport::call(const std::string& url, const http::Call& c, const std::vector<std::string>& headers,
                       const std::string& pin)
{
    if ((c.method.empty() || c.method == "GET") && c.body.empty() && c.fields.empty() && c.file_path.empty() &&
        c.to_path.empty())
        return get(url, headers, pin);
    Answer a;
    a.error = "not supported";
    return a;
}

namespace
{
// Ownfoil 2: its owned apps, a page at a time, and its handshake for its
// name and message of the day. Nothing (`f` untouched) when the server
// does not answer GraphQL; a refusal of the credentials is the last word.
enum class GraphQl { Absent, Ok, Failed };
GraphQl read_graphql(const std::string& endpoint, const std::vector<std::string>& headers, const std::string& pin,
                     Transport& t, Fetch* f)
{
    Catalog  c;
    uint64_t total = 0;
    for (int page = 1; page <= GRAPHQL_MAX_PAGES; page++) {
        const Answer a = t.get(endpoint + "/api/graphql?query=" + graphql_apps_query(page, GRAPHQL_PAGE), headers, pin);
        if (a.status == 401 || a.status == 403) {
            f->why = refusal(a);
            return GraphQl::Failed;
        }
        std::string err;
        const size_t before = c.items.size() + c.skipped;
        if (a.status != 200 || !add_graphql_apps(c, a.body, endpoint, &total, &err)) {
            if (page == 1) return GraphQl::Absent;
            f->why = a.status != 200 ? refusal(a) : err;
            return GraphQl::Failed;
        }
        // The last page: short, or everything there.
        if (c.items.size() + c.skipped - before < (size_t)GRAPHQL_PAGE || c.items.size() + c.skipped >= total ||
            c.items.size() >= MAX_ITEMS)
            break;
    }
    http::Call hello;
    hello.method = "OPTIONS";
    const Answer h = t.call(endpoint + "/", hello, headers, pin);
    if (h.status == 200)
        if (const auto info = parse_handshake(h.body)) {
            f->info = *info;
            c.motd = info->motd;
        }
    f->status = FetchStatus::Ok;
    f->format = Format::Api;
    f->dialect = Dialect::GraphQl;
    f->catalog = std::move(c);
    return GraphQl::Ok;
}
}   // namespace

bool same_origin(const std::string& a, const std::string& b)
{
    auto origin = [](const std::string& url) {
        const size_t scheme = url.find("://");
        if (scheme == std::string::npos) return std::string();
        const size_t end = url.find_first_of("/?#", scheme + 3);
        std::string  o = url.substr(0, end);
        for (char& c : o) c = (char)std::tolower((unsigned char)c);
        // Credentials in the address ("user@host") are not part of it.
        const size_t at = o.find('@', scheme + 3);
        if (at != std::string::npos) o.erase(scheme + 3, at - scheme - 2);
        if (o.size() > 4 && o.compare(o.size() - 4, 4, ":443") == 0 && o.starts_with("https://")) o.resize(o.size() - 4);
        return o;
    };
    const std::string oa = origin(a);
    return !oa.empty() && oa == origin(b);
}

std::vector<std::string> auth_headers(const std::string& user, const std::string& password)
{
    if (user.empty()) return {};
    const std::string both = user + ":" + password;
    return {"Authorization: Basic " + base64((const uint8_t*)both.data(), both.size())};
}

Fetch fetch(const Server& server, const std::string& password, Transport& t)
{
    const Endpoints   e{server.lan_url, server.wan_url, server.connection};
    const std::string endpoint = without_slash(choose_endpoint(e, [&](const std::string& u) { return t.reachable(u); }));
    if (!endpoint.starts_with("https://")) return failed("the server has no https:// address");
    const bool        on_lan = !server.lan_url.empty() && endpoint == without_slash(server.lan_url);
    const std::string pin = on_lan ? server.pin : "";
    const auto        headers = auth_headers(server.username, password);

    // A first request, to the API or to the index: a LAN server's own
    // certificate shows up there. The newer /sections path is tried first,
    // so it is what carries the TLS test.
    const std::string limit = "?limit=" + std::to_string(SECTIONS_LIMIT);
    const std::string first_url = server.format == Format::Index ? endpoint : endpoint + "/api/remote/sections" + limit;
    const Answer      first = t.get(first_url, headers, pin);
    if (first.tls_failed) {
        if (!on_lan) return failed("the server's certificate is not one a recognized authority vouches for");
        if (!pin.empty()) return failed("the server's key is no longer the one trusted: it changed, or it is another server");
        // Pin on first use: the key it showed, for the user to compare.
        std::string why;
        const std::string pem = t.certificate(endpoint, &why);
        const auto        der = pem_to_der(pem);
        const auto        h = der ? spki_sha256(der->data(), der->size()) : std::nullopt;
        if (!h) return failed(why.empty() ? "the server's certificate cannot be read" : why);
        Fetch f;
        f.status = FetchStatus::UnknownCertificate;
        f.endpoint = endpoint;
        f.on_lan = true;
        f.offered_pin = pin_text(*h);
        f.offered_short = pin_short(*h);
        f.why = "the server's certificate is its own: trust its key?";
        return f;
    }

    Fetch f;
    f.endpoint = endpoint;
    f.on_lan = on_lan;
    if (server.format != Format::Index) {
        // The API paths, newest first (AeroFoil/Ownfoil's /sections, as
        // Tinfoil and CyberFoil ask for them), then the older ones. The
        // first answer is the TLS carrier above, so it is not asked twice.
        static const char* const api_paths[] = {"/api/remote/sections", "/api/shop/sections", "/api/remote",
                                                 "/api/shop"};
        std::string not_api;   // a 200 that was no catalogue: why
        for (size_t i = 0; i < sizeof api_paths / sizeof *api_paths; i++) {
            const bool   sections = i < 2;
            const Answer a = i == 0 ? first : t.get(endpoint + api_paths[i] + (sections ? limit : ""), headers, pin);
            if (a.status == 200) {
                std::string err;
                if (auto c = parse_api(a.body, endpoint, &err)) {
                    // A frozen account: only its placeholders.
                    if (c->frozen && c->items.empty()) return failed("the account is frozen on this server");
                    f.status = FetchStatus::Ok;
                    f.format = Format::Api;
                    f.dialect = sections ? Dialect::Sections : Dialect::Shop;
                    f.catalog = std::move(*c);
                    return f;
                }
                // A 200 that is not a catalogue: Ownfoil 2 (its web page
                // for any other path) is asked below; then, for an explicit
                // Api, that error is the last word; in Auto, the address
                // may still be a JSON index, read below.
                not_api = err;
                break;
            }
            // 404: this path is not served, so try the next one. Any other
            // answer (a refused credential, a server error) is the server's
            // word: do not guess further paths or fall back to the index.
            if (a.status != 404) return failed(refusal(a));
        }
        switch (read_graphql(endpoint, headers, pin, t, &f)) {
        case GraphQl::Ok: return f;
        case GraphQl::Failed: return failed(f.why);
        case GraphQl::Absent: break;
        }
        // No API answered. An explicit Api has no catalogue; Auto reads the
        // address as a JSON index below.
        if (server.format == Format::Api)
            return failed(!not_api.empty() ? not_api : "the server has no catalogue at /api/remote, /api/shop or /api/graphql (HTTP 404)");
    }
    // A JSON index at the address itself.
    std::string why;
    Catalog     c;
    const bool  read = server.format == Format::Index ? read_indexes(endpoint, headers, pin, t, &first, &c, &why)
                                                      : read_indexes(endpoint, headers, pin, t, nullptr, &c, &why);
    if (!read) return failed(why);
    f.status = FetchStatus::Ok;
    f.format = Format::Index;
    f.catalog = std::move(c);
    return f;
}

}   // namespace library
