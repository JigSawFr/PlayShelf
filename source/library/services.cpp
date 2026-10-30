// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/services.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>

#include <aurore/extern/nlohmann/json.hpp>

#include "util/json_util.hpp"

namespace library
{
namespace
{
using json = nlohmann::json;
using json_util::array_at;
using json_util::string_at;

constexpr size_t MAX_TEXT = 256, MAX_URL = 2048, MAX_SERVER_WORDS = 200;

bool fail(std::string* why, const std::string& text)
{
    if (why) *why = text;
    return false;
}

// Parsed with the catalogue's depth limit.
bool parse_json(const std::string& body, json* out)
{
    bool deep = false;
    *out = json::parse(
        body,
        [&](int depth, json::parse_event_t, json&) {
            if (depth > MAX_DEPTH) deep = true;
            return depth <= MAX_DEPTH;
        },
        false);
    return !deep && !out->is_discarded();
}

// A title id as AeroFoil writes them: 16 hex digits.
bool hex_id(const std::string& text, uint64_t* out)
{
    if (text.size() != 16) return false;
    uint64_t v = 0;
    for (char c : text) {
        if (!std::isxdigit((unsigned char)c)) return false;
        v = v << 4 | (uint64_t)(std::isdigit((unsigned char)c) ? c - '0' : std::tolower((unsigned char)c) - 'a' + 10);
    }
    *out = v;
    return v != 0;
}

std::string id_text(uint64_t id)
{
    static const char* const hex = "0123456789ABCDEF";
    std::string              out(16, '0');
    for (int i = 15; i >= 0; i--, id >>= 4)
        out[(size_t)i] = hex[id & 15];
    return out;
}

uint64_t u64_or(const json& j, const char* key)
{
    const auto it = j.find(key);
    if (it == j.end()) return 0;
    if (it->is_number_unsigned()) return it->get<uint64_t>();
    if (it->is_number_integer() && it->get<int64_t>() > 0) return (uint64_t)it->get<int64_t>();
    return 0;
}

std::string text_at(const json& j, const char* key, size_t max = MAX_TEXT)
{
    return display_text(string_at(j, key), max);
}

// The server's words in an error answer ({"error": …}, {"message": …}),
// else what its status says.
std::string refusal(const Answer& a)
{
    if (a.status == 0) return a.error.empty() ? "the server does not answer" : a.error;
    json j;
    if (parse_json(a.body, &j) && j.is_object())
        for (const char* key : {"error", "message"})
            if (!string_at(j, key).empty()) return display_text(string_at(j, key), MAX_SERVER_WORDS);
    if (a.status == 401) return "the server refused the username or password (HTTP 401)";
    if (a.status == 403) return "the server does not allow it for this account (HTTP 403)";
    if (a.status == 404) return "the server does not offer it (HTTP 404)";
    return "the server answered HTTP " + std::to_string(a.status);
}

// A request of the signed-in pages: the session's cookie with the
// credentials, redirects not followed (the server sends one to its sign-in
// page when the session is over: signed in again, once).
Answer with_session(const Link& link, Transport& t, Session& session, const std::string& path, http::Call c,
                    std::string* why, bool* ok)
{
    *ok = false;
    c.follow = false;
    Answer a;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (session.cookie.empty() && !sign_in(link, t, &session, why)) return a;
        std::vector<std::string> headers = link.headers;
        headers.push_back("Cookie: " + session.cookie);
        a = t.call(link.endpoint + path, c, headers, link.pin);
        const bool signed_out = a.status == 401 || ((a.status == 301 || a.status == 302 || a.status == 303) &&
                                                    a.location.contains("/login"));
        if (!signed_out) break;
        session.cookie.clear();
        if (attempt == 1) {
            fail(why, "the server's sign-in did not hold");
            return a;
        }
    }
    if (a.status < 200 || a.status >= 300) {
        fail(why, refusal(a));
        return a;
    }
    *ok = true;
    return a;
}
}   // namespace

Link link_of(const Server& server, const Fetch& fetch, const std::string& password)
{
    Link l;
    l.endpoint = fetch.endpoint;
    while (l.endpoint.size() > 8 && l.endpoint.back() == '/')
        l.endpoint.pop_back();
    l.headers = auth_headers(server.username, password);
    l.pin = fetch.on_lan ? server.pin : "";
    l.username = server.username;
    l.password = password;
    return l;
}

bool sign_in(const Link& link, Transport& t, Session* session, std::string* why)
{
    if (link.username.empty()) return fail(why, "the server needs a username for this: add one to the server");
    http::Call c;
    c.method = "POST";
    c.content_type = "application/x-www-form-urlencoded";
    c.body = "user=" + url_encode(link.username) + "&password=" + url_encode(link.password);
    c.follow = false;
    const Answer a = t.call(link.endpoint + "/login", c, {}, link.pin);
    if (a.status == 0) return fail(why, a.error.empty() ? "the server does not answer" : a.error);
    // Signed in: a redirect away from the sign-in page, with its cookie.
    const bool moved = a.status == 301 || a.status == 302 || a.status == 303;
    if (!moved || a.location.contains("/login") || a.cookies.empty())
        return fail(why, a.status == 429 || a.status == 403
                             ? "the server refused the sign-in for now (HTTP " + std::to_string(a.status) + ")"
                             : moved || a.status == 200 ? "the server refused the username or password"
                                                        : refusal(a));
    std::string cookie;
    for (const std::string& c2 : a.cookies) {
        if (cookie.size() + c2.size() > 8192) break;
        cookie += (cookie.empty() ? "" : "; ") + c2;
    }
    session->cookie = cookie;
    return true;
}

// ---- Cheats

std::string normal_build_id(const std::string& build_id)
{
    if (build_id.empty() || build_id.size() > 64) return "";
    std::string out;
    for (char c : build_id) {
        if (!std::isxdigit((unsigned char)c)) return "";
        out += (char)std::toupper((unsigned char)c);
    }
    return out;
}

std::string cheat_path(const std::string& sd_root, uint64_t title_id, const std::string& build_id)
{
    const std::string bid = normal_build_id(build_id);
    if (bid.empty() || !title_id) return "";
    std::string root = sd_root;
    while (!root.empty() && root.back() == '/')
        root.pop_back();
    return root + "/atmosphere/contents/" + id_text(title_id) + "/cheats/" + bid + ".txt";
}

std::vector<std::string> installed_cheats(const std::string& sd_root, uint64_t title_id)
{
    std::vector<std::string> out;
    const std::string        any = cheat_path(sd_root, title_id, "0");
    if (any.empty()) return out;
    const std::string dir = any.substr(0, any.find_last_of('/'));
    DIR*              d = ::opendir(dir.c_str());
    if (!d) return out;
    while (const dirent* e = ::readdir(d)) {
        const std::string name = e->d_name;
        if (name.size() < 5 || name.compare(name.size() - 4, 4, ".txt") != 0) continue;
        const std::string bid = normal_build_id(name.substr(0, name.size() - 4));
        if (!bid.empty()) out.push_back(bid);
    }
    ::closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

std::optional<std::vector<Cheat>> parse_cheats(const std::string& body, const std::string& base_url, std::string* why)
{
    json j;
    if (!parse_json(body, &j) || !j.is_object()) {
        fail(why, "the server's answer is not a list of cheats");
        return std::nullopt;
    }
    const auto success = j.find("success");
    if (success != j.end() && success->is_boolean() && !success->get<bool>()) {
        fail(why, string_at(j, "error").empty() ? "the server refused" : display_text(string_at(j, "error"), MAX_SERVER_WORDS));
        return std::nullopt;
    }
    const json* list = array_at(j, "cheats");
    if (!list) {
        fail(why, "the server's answer is not a list of cheats");
        return std::nullopt;
    }
    std::vector<Cheat> out;
    for (const json& e : *list) {
        if (!e.is_object() || out.size() >= MAX_SERVICE_ITEMS) continue;
        Cheat c;
        c.build_id = normal_build_id(string_at(e, "build_id"));
        const std::string ref = string_at(e, "url");
        c.url = ref.size() <= MAX_URL ? resolve(base_url, ref) : std::string();
        if (!hex_id(string_at(e, "title_id"), &c.title_id) || c.build_id.empty() || c.url.empty()) continue;
        c.title_name = text_at(e, "title_name");
        if (c.title_name.empty()) c.title_name = id_text(c.title_id);
        c.note = text_at(e, "note", 1000);
        c.size = u64_or(e, "size");
        out.push_back(std::move(c));
    }
    return out;
}

bool list_cheats(const Link& link, Transport& t, Session& session, std::vector<Cheat>* out, std::string* why)
{
    bool         ok = false;
    const Answer a = with_session(link, t, session, "/api/cheats", http::Call(), why, &ok);
    if (!ok) return false;
    auto list = parse_cheats(a.body, link.endpoint, why);
    if (!list) return false;
    *out = std::move(*list);
    return true;
}

bool fetch_cheat(const Link& link, Transport& t, const Cheat& cheat, std::string* text, std::string* why)
{
    // The credentials go with it only to their own server.
    const bool   own = same_origin(cheat.url, link.endpoint);
    const Answer a = t.get(cheat.url, own ? link.headers : std::vector<std::string>(), own ? link.pin : "");
    if (a.status != 200) return fail(why, refusal(a));
    if (a.body.size() > MAX_CHEAT_BYTES) return fail(why, "the cheat file is too large");
    // A JSON error answered with a 200 (a Tinfoil-style refusal): said.
    json j;
    if (!a.body.empty() && a.body[0] == '{' && parse_json(a.body, &j) && j.is_object() && !string_at(j, "error").empty())
        return fail(why, display_text(string_at(j, "error"), MAX_SERVER_WORDS));
    *text = a.body;
    return true;
}

// ---- Save backups

std::optional<std::vector<SaveBackup>> parse_saves(const std::string& body, const std::string& base_url, std::string* why)
{
    json j;
    if (!parse_json(body, &j) || !j.is_object()) {
        fail(why, "the server's answer is not a list of save backups");
        return std::nullopt;
    }
    const json* list = array_at(j, "saves");
    if (!list) {
        fail(why, string_at(j, "error").empty() ? "the server's answer is not a list of save backups"
                                                : display_text(string_at(j, "error"), MAX_SERVER_WORDS));
        return std::nullopt;
    }
    std::vector<SaveBackup> out;
    for (const json& e : *list) {
        if (!e.is_object() || out.size() >= MAX_SERVICE_ITEMS) continue;
        SaveBackup s;
        const std::string down = string_at(e, "download_url");
        s.download_url = down.size() <= MAX_URL ? resolve(base_url, down) : std::string();
        if (!hex_id(string_at(e, "title_id"), &s.title_id) || s.download_url.empty()) continue;
        const std::string del = string_at(e, "delete_url");
        if (!del.empty() && del.size() <= MAX_URL) s.delete_url = resolve(base_url, del);
        s.save_id = text_at(e, "save_id", 128);
        s.title_name = text_at(e, "title_name");
        if (s.title_name.empty()) s.title_name = id_text(s.title_id);
        s.note = text_at(e, "note", MAX_NOTE);
        s.created_at = text_at(e, "created_at", 64);
        s.created = (int64_t)u64_or(e, "created_ts");
        s.size = u64_or(e, "size");
        s.latest = json_util::flag_at(e, "is_latest");
        s.server_copy = json_util::flag_at(e, "latest_directory");
        if (s.server_copy) s.delete_url.clear();
        out.push_back(std::move(s));
    }
    // By game, then newest first; the server's own copy after the backups.
    std::stable_sort(out.begin(), out.end(), [](const SaveBackup& a, const SaveBackup& b) {
        if (a.title_id != b.title_id) return a.title_id < b.title_id;
        if (a.server_copy != b.server_copy) return !a.server_copy;
        return a.created > b.created;
    });
    return out;
}

bool list_saves(const Link& link, Transport& t, std::vector<SaveBackup>* out, std::string* why)
{
    const Answer a = t.get(link.endpoint + "/api/saves/list", link.headers, link.pin);
    if (a.status != 200) return fail(why, a.status == 403 && refusal(a).find("Backup access") != std::string::npos
                                              ? "this account has no Backup permission on the server"
                                              : refusal(a));
    auto list = parse_saves(a.body, link.endpoint, why);
    if (!list) return false;
    *out = std::move(*list);
    return true;
}

bool upload_save(const Link& link, Transport& t, uint64_t title_id, const std::string& zip_path, const std::string& note,
                 const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel,
                 std::string* why)
{
    if (!title_id) return fail(why, "no game");
    const std::string tid = id_text(title_id);
    http::Call        c;
    c.method = "POST";
    c.fields = { { "title_id", tid }, { "application_id", tid } };
    const std::string n = display_text(note, MAX_NOTE);
    if (!n.empty()) c.fields.emplace_back("note", n);
    c.file_field = "file";
    c.file_path = zip_path;
    c.file_name = tid + ".zip";
    c.progress = progress;
    c.cancel = cancel;
    const Answer a = t.call(link.endpoint + "/api/saves/upload/" + tid, c, link.headers, link.pin);
    if (a.status < 200 || a.status >= 300) return fail(why, refusal(a));
    return true;
}

bool download_save(const Link& link, Transport& t, const SaveBackup& backup, const std::string& zip_path,
                   const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel,
                   std::string* why)
{
    if (!same_origin(backup.download_url, link.endpoint)) return fail(why, "the backup is on another server");
    http::Call c;
    c.to_path = zip_path;
    c.progress = progress;
    c.cancel = cancel;
    const Answer a = t.call(backup.download_url, c, link.headers, link.pin);
    if (a.status != 200) return fail(why, refusal(a));
    return true;
}

bool delete_save(const Link& link, Transport& t, const SaveBackup& backup, std::string* why)
{
    if (backup.delete_url.empty()) return fail(why, "the server keeps this copy itself");
    if (!same_origin(backup.delete_url, link.endpoint)) return fail(why, "the backup is on another server");
    http::Call c;
    c.method = "DELETE";
    const Answer a = t.call(backup.delete_url, c, link.headers, link.pin);
    if (a.status < 200 || a.status >= 300) return fail(why, refusal(a));
    return true;
}

// ---- Requests

Parsed<std::vector<TitleRequest>> parse_requests(const std::string& body)
{
    json j;
    if (!parse_json(body, &j) || !j.is_object() || !array_at(j, "requests")) {
        return std::unexpected("the server's answer is not a list of requests");
    }
    std::vector<TitleRequest> out;
    for (const json& e : *array_at(j, "requests")) {
        if (!e.is_object() || out.size() >= MAX_SERVICE_ITEMS) continue;
        TitleRequest r;
        if (!hex_id(string_at(e, "title_id"), &r.title_id)) continue;
        json_util::integer_at(e, "id", &r.id);
        json_util::integer_at(e, "created_at", &r.created);
        r.title_name = text_at(e, "title_name");
        r.status = text_at(e, "status", 32);
        out.push_back(std::move(r));
    }
    std::stable_sort(out.begin(), out.end(), [](const TitleRequest& a, const TitleRequest& b) { return a.created > b.created; });
    return out;
}

bool list_requests(const Link& link, Transport& t, Session& session, std::vector<TitleRequest>* out, std::string* why)
{
    bool         ok = false;
    const Answer a = with_session(link, t, session, "/api/requests", http::Call(), why, &ok);
    if (!ok) return false;
    auto list = parse_requests(a.body);
    if (!list) return fail(why, list.error());
    *out = std::move(*list);
    return true;
}

bool request_title(const Link& link, Transport& t, Session& session, uint64_t title_id, const std::string& name,
                   std::string* message, std::string* why)
{
    if (!title_id) return fail(why, "no game");
    http::Call c;
    c.method = "POST";
    c.content_type = "application/json";
    json body = { { "title_id", id_text(title_id) } };
    if (!name.empty()) body["title_name"] = display_text(name, MAX_TEXT);
    c.body = body.dump();
    bool         ok = false;
    const Answer a = with_session(link, t, session, "/api/requests", c, why, &ok);
    if (!ok) return false;
    json j;
    if (!parse_json(a.body, &j) || !j.is_object()) return fail(why, "the server's answer is not one to a request");
    const std::string words = display_text(string_at(j, "message"), MAX_SERVER_WORDS);
    if (!json_util::flag_at(j, "success")) return fail(why, words.empty() ? "the server refused the request" : words);
    if (message) *message = words;
    return true;
}

}   // namespace library
