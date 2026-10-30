// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/catalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>

#include <aurore/extern/nlohmann/json.hpp>

#include "util/number.hpp"

namespace library
{
namespace
{
using json = nlohmann::json;

constexpr size_t MAX_NAME = 256, MAX_TITLE = 96, MAX_MOTD = 2048, MAX_URL = 2048, MAX_DIRECTORIES = 64;

bool fail(std::string* why, const std::string& text)
{
    if (why) *why = text;
    return false;
}

// Parsed with a depth limit: a deep answer cannot exhaust the stack.
bool parse_json(const std::string& body, json* out)
{
    bool too_deep = false;
    *out = json::parse(
        body,
        [&](int depth, json::parse_event_t, json&) {
            if (depth > MAX_DEPTH) too_deep = true;
            return depth <= MAX_DEPTH;
        },
        false);
    return !too_deep && !out->is_discarded();
}

std::string str_at(const json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

bool hex_u64(const std::string& text, uint64_t* out)
{
    std::string t = text;
    while (!t.empty() && std::isspace((unsigned char)t.front()))
        t.erase(t.begin());
    while (!t.empty() && std::isspace((unsigned char)t.back()))
        t.pop_back();
    if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) t = t.substr(2);
    if (t.empty() || t.size() > 16) return false;
    uint64_t v = 0;
    for (char c : t) {
        if (!std::isxdigit((unsigned char)c)) return false;
        v = v << 4 | (uint64_t)(std::isdigit((unsigned char)c) ? c - '0' : std::tolower((unsigned char)c) - 'a' + 10);
    }
    *out = v;
    return true;
}

std::string fmt_id(uint64_t id)
{
    static const char* const hex = "0123456789ABCDEF";
    std::string              out(16, '0');
    for (int i = 15; i >= 0; i--, id >>= 4)
        out[(size_t)i] = hex[id & 15];
    return out;
}

bool u64_at(const json& j, const char* key, uint64_t* out)
{
    const auto it = j.find(key);
    if (it == j.end()) return false;
    if (it->is_number_unsigned()) {
        *out = it->get<uint64_t>();
        return true;
    }
    if (it->is_number_integer() && it->get<int64_t>() >= 0) {
        *out = (uint64_t)it->get<int64_t>();
        return true;
    }
    return false;
}

bool title_id_at(const json& j, uint64_t* out)
{
    for (const char* key : {"title_id", "titleId", "id"}) {
        const auto it = j.find(key);
        if (it == j.end()) continue;
        if (it->is_string() && hex_u64(it->get<std::string>(), out)) return *out != 0;
        if (it->is_number_unsigned()) {
            *out = it->get<uint64_t>();
            return *out != 0;
        }
    }
    return false;
}

bool version_at(const json& j, uint32_t* out)
{
    for (const char* key : {"app_version", "version"}) {
        uint64_t v;
        if (u64_at(j, key, &v) && v <= 0xffffffffu) {
            *out = (uint32_t)v;
            return true;
        }
        const auto it = j.find(key);
        if (it != j.end() && it->is_string()) {
            if (const auto n = number::parse<uint32_t>(it->get_ref<const std::string&>())) {
                *out = *n;
                return true;
            }
        }
    }
    return false;
}

uint32_t date_at(const json& j)
{
    for (const char* key : {"release_date", "releaseDate"}) {
        uint64_t n;
        if (u64_at(j, key, &n) && n >= 19000101 && n <= 29991231) return (uint32_t)n;
        const std::string s = str_at(j, key);
        std::string       digits;
        for (char c : s)
            if (std::isdigit((unsigned char)c)) digits += c;
        if (digits.size() >= 8) {
            const uint32_t v = *number::parse<uint32_t>(std::string_view(digits).substr(0, 8));   // digits only
            if (v >= 19000101 && v <= 29991231) return v;
        }
    }
    return 0;
}

Kind kind_from_text(std::string t)
{
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (t == "base" || t == "game" || t == "application" || t == "app") return Kind::Base;
    if (t == "update" || t == "updates" || t == "upd" || t == "patch") return Kind::Update;
    if (t == "dlc" || t == "dlcs" || t == "addon" || t == "add-on" || t == "aoc") return Kind::Dlc;
    return Kind::Unknown;
}

Kind kind_at(const json& j)
{
    const auto it = j.find("app_type");
    if (it == j.end()) return Kind::Unknown;
    if (it->is_string()) return kind_from_text(it->get<std::string>());
    if (it->is_number_integer()) {
        // NcmContentMetaType: 0x80 application, 0x81 patch, 0x82 add-on.
        switch (it->get<int64_t>()) {
        case 0x80: return Kind::Base;
        case 0x81: return Kind::Update;
        case 0x82: return Kind::Dlc;
        default: return Kind::Unknown;
        }
    }
    return Kind::Unknown;
}

// The file name of an address: its last segment, %-decoded, before "?".
std::string file_name(const std::string& url)
{
    std::string s = url;
    const size_t q = s.find_first_of("?#");
    if (q != std::string::npos) s.resize(q);
    const size_t slash = s.find_last_of('/');
    if (slash != std::string::npos) s = s.substr(slash + 1);
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) && std::isxdigit((unsigned char)s[i + 2])) {
            out += (char)*number::parse<int>(std::string_view(s).substr(i + 1, 2), 16);   // two hex digits, checked above
            i += 2;
        } else {
            out += s[i] == '+' ? ' ' : s[i];
        }
    }
    return out;
}

// One item from an entry: an address (a string, or an object's "url"),
// the rest of its fields when it is an object.
bool add_item(Catalog& c, const json& entry, const std::string& base, const std::string& section, std::set<std::string>& seen)
{
    if (c.items.size() >= MAX_ITEMS) {
        c.skipped++;
        return false;
    }
    std::string ref;
    if (entry.is_string())
        ref = entry.get<std::string>();
    else if (entry.is_object())
        for (const char* key : {"url", "path", "file", "download_url"})
            if ((ref = str_at(entry, key)).size()) break;
    if (ref.empty() || ref.size() > MAX_URL || ref.starts_with("gdrive:")) {
        c.skipped++;
        return false;
    }
    std::string fragment;
    const size_t hash = ref.find('#');
    if (hash != std::string::npos) {
        fragment = ref.substr(hash + 1);
        ref.resize(hash);
    }
    Item item;
    item.url = resolve(base, ref);
    if (item.url.empty() || !seen.insert(item.url).second) {
        c.skipped++;
        return false;
    }
    item.section = section;
    std::string name = entry.is_object() ? str_at(entry, "name") : std::string();
    if (name.empty()) name = !fragment.empty() ? file_name("/" + fragment) : file_name(item.url);
    item.name = display_text(name, MAX_NAME);
    if (item.name.empty()) {
        c.skipped++;
        return false;
    }
    if (entry.is_object()) {
        u64_at(entry, "size", &item.size);
        title_id_at(entry, &item.title_id);
        item.has_version = version_at(entry, &item.version);
        item.kind = kind_at(entry);
        item.release_date = date_at(entry);
        for (const char* key : {"icon_url", "iconUrl"}) {
            const std::string icon = str_at(entry, key);
            if (!icon.empty() && icon.size() <= MAX_URL && (item.icon_url = resolve(base, icon)).size()) break;
        }
    }
    // The tags of the name, else of the file's name.
    uint64_t tid = 0;
    uint32_t ver = 0;
    bool     has_ver = false;
    bool tagged = parse_name_tags(item.name, &tid, &ver, &has_ver);
    if (!tagged || !has_ver) {
        uint64_t   ftid = 0;
        uint32_t   fver = 0;
        bool       fhas = false;
        const bool ftagged = parse_name_tags(!fragment.empty() ? fragment : file_name(item.url), &ftid, &fver, &fhas);
        if (!tagged && ftagged) {
            tid = ftid;
            tagged = true;
        }
        if (!has_ver && fhas) {
            ver = fver;
            has_ver = true;
        }
    }
    if (tagged || has_ver) {
        if (!item.title_id && tagged) item.title_id = tid;
        if (!item.has_version && has_ver) {
            item.version = ver;
            item.has_version = true;
        }
    }
    if (item.kind == Kind::Unknown && item.title_id) item.kind = kind_of(item.title_id);
    if (item.kind == Kind::Unknown) item.kind = kind_from_text(section);
    c.items.push_back(std::move(item));
    return true;
}
}   // namespace

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

bool parse_name_tags(const std::string& name, uint64_t* title_id, uint32_t* version, bool* has_version)
{
    bool found = false;
    *has_version = false;
    for (size_t open = name.find('['); open != std::string::npos; open = name.find('[', open + 1)) {
        const size_t close = name.find(']', open);
        if (close == std::string::npos) break;
        const std::string tag = name.substr(open + 1, close - open - 1);
        uint64_t          v;
        if (tag.size() == 16 && hex_u64(tag, &v) && v) {
            *title_id = v;
            found = true;
        } else if (tag.size() >= 2 && (tag[0] == 'v' || tag[0] == 'V') &&
                   std::all_of(tag.begin() + 1, tag.end(), [](unsigned char c) { return std::isdigit(c); }) && tag.size() <= 11) {
            if (const auto n = number::parse<uint32_t>(std::string_view(tag).substr(1))) {
                *version = *n;
                *has_version = true;
            }
        }
    }
    return found;
}

std::optional<Catalog> parse_api(const std::string& body, const std::string& base_url, std::string* why)
{
    json j;
    if (!parse_json(body, &j) || !j.is_object()) {
        fail(why, "the server's answer is not a catalogue");
        return std::nullopt;
    }
    const std::string error = str_at(j, "error");
    if (!error.empty()) {
        fail(why, display_text(error, MAX_TITLE * 2));
        return std::nullopt;
    }
    const auto sections = j.find("sections");
    if (sections == j.end() || !sections->is_array()) {
        fail(why, "the server's answer has no sections");
        return std::nullopt;
    }
    Catalog               c;
    std::set<std::string> seen;
    c.motd = display_text(str_at(j, "motd").empty() ? str_at(j, "success") : str_at(j, "motd"), MAX_MOTD);
    for (const json& s : *sections) {
        if (!s.is_object()) continue;
        const auto items = s.find("items");
        if (items == s.end() || !items->is_array()) continue;
        Section sec;
        sec.id = display_text(str_at(s, "id").empty() ? "all" : str_at(s, "id"), MAX_TITLE);
        sec.title = display_text(str_at(s, "title").empty() ? sec.id : str_at(s, "title"), MAX_TITLE);
        const auto truncated = s.find("truncated");
        if (truncated != s.end() && truncated->is_boolean() && truncated->get<bool>()) c.truncated = true;
        for (const json& e : *items) {
            // Cheats, AeroFoil's, are no package to install (services.hpp
            // reads them).
            if (sec.id == "cheats" || (e.is_object() && str_at(e, "app_type") == "CHEAT")) continue;
            // A frozen account's placeholder, in every section: no game.
            if (e.is_object() && str_at(e, "url").rfind("/api/frozen/notice", 0) == 0) {
                c.frozen = true;
                continue;
            }
            if (add_item(c, e, base_url, sec.id, seen)) sec.items.push_back(c.items.size() - 1);
        }
        c.sections.push_back(std::move(sec));
    }
    return c;
}

std::optional<ServerInfo> parse_handshake(const std::string& body)
{
    json j;
    if (!parse_json(body, &j) || !j.is_object() || str_at(j, "uid").empty() || !j.contains("name")) return std::nullopt;
    ServerInfo info;
    info.name = display_text(str_at(j, "name"), MAX_TITLE);
    info.version = display_text(str_at(j, "version"), 24);
    info.motd = display_text(str_at(j, "motd"), MAX_MOTD);
    uint64_t protocol = 0;
    if (u64_at(j, "protocol_version", &protocol) && protocol < 1000) info.protocol = (int)protocol;
    const auto pub = j.find("public");
    info.public_shop = pub != j.end() && pub->is_boolean() && pub->get<bool>();
    const auto features = j.find("features");
    if (features != j.end() && features->is_object()) {
        const auto saves = features->find("save_backup");
        info.save_backup = saves != features->end() && saves->is_boolean() && saves->get<bool>();
    }
    return info;
}

std::string url_encode(const std::string& s)
{
    static const char* const hex = "0123456789ABCDEF";
    std::string              out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string graphql_apps_query(int page, int page_size)
{
    // The owned apps with what an item needs: the file's address and size,
    // the app's id, version and kind, its game's name and icon (a DLC's own
    // name from its titledb entry).
    const std::string q = "{apps(owned:true,page:" + std::to_string(page) + ",pageSize:" + std::to_string(page_size) +
                          "){total items{appId titleId appVersion appType releaseDate downloadUrl downloadSize "
                          "downloadExtension titledb{name} title{name icon(size:THUMB){url}}}}}";
    return url_encode(q);
}

bool add_graphql_apps(Catalog& c, const std::string& body, const std::string& base_url, uint64_t* total, std::string* why)
{
    json j;
    if (!parse_json(body, &j) || !j.is_object()) return fail(why, "the server's answer is not GraphQL");
    const auto errors = j.find("errors");
    if (errors != j.end() && errors->is_array() && !errors->empty()) {
        const json& e = (*errors)[0];
        return fail(why, e.is_object() && !str_at(e, "message").empty() ? display_text(str_at(e, "message"), MAX_TITLE * 2)
                                                                         : "the server refused the query");
    }
    const auto data = j.find("data");
    if (data == j.end() || !data->is_object()) return fail(why, "the server's answer has no data");
    const auto apps = data->find("apps");
    if (apps == data->end() || !apps->is_object()) return fail(why, "the server's answer has no apps");
    *total = 0;
    u64_at(*apps, "total", total);
    const auto items = apps->find("items");
    if (items == apps->end() || !items->is_array()) return fail(why, "the server's answer has no apps");
    std::set<std::string> seen;
    for (const Item& i : c.items)
        seen.insert(i.url);
    for (const json& a : *items) {
        if (!a.is_object()) continue;
        if (c.items.size() >= MAX_ITEMS) {
            c.skipped++;
            continue;
        }
        const std::string ref = str_at(a, "downloadUrl");
        Item              item;
        item.url = ref.size() <= MAX_URL ? resolve(base_url, ref) : std::string();
        uint64_t id = 0;
        if (item.url.empty() || !hex_u64(str_at(a, "appId"), &id) || !id || !seen.insert(item.url).second) {
            c.skipped++;
            continue;
        }
        item.title_id = id;
        u64_at(a, "downloadSize", &item.size);
        item.has_version = version_at(a, &item.version);
        if (!item.has_version) {
            uint64_t v;
            if (u64_at(a, "appVersion", &v) && v <= 0xffffffffu) {
                item.version = (uint32_t)v;
                item.has_version = true;
            }
        }
        item.kind = kind_from_text(str_at(a, "appType"));
        if (item.kind == Kind::Unknown) item.kind = kind_of(id);
        item.release_date = date_at(a);
        const auto title = a.find("title");
        const auto own = a.find("titledb");
        std::string name;
        if (item.kind == Kind::Dlc && own != a.end() && own->is_object()) name = str_at(*own, "name");
        if (name.empty() && title != a.end() && title->is_object()) name = str_at(*title, "name");
        if (title != a.end() && title->is_object()) {
            const auto icon = title->find("icon");
            if (icon != title->end() && icon->is_object()) {
                const std::string u = str_at(*icon, "url");
                if (!u.empty() && u.size() <= MAX_URL) item.icon_url = resolve(base_url, u);
            }
        }
        if (name.empty()) name = fmt_id(id);
        item.name = display_text(name, MAX_NAME);
        c.items.push_back(std::move(item));
    }
    return true;
}

std::optional<Catalog> parse_index(const std::string& body, const std::string& base_url, std::string* why)
{
    json j;
    if (!parse_json(body, &j) || !j.is_object()) {
        fail(why, "the index is not JSON");
        return std::nullopt;
    }
    if (!j.contains("files") && !j.contains("directories") && !j.contains("titledb") && !j.contains("success")) {
        fail(why, "the index has none of files, directories, titledb, success");
        return std::nullopt;
    }
    Catalog               c;
    std::set<std::string> seen;
    c.motd = display_text(str_at(j, "success"), MAX_MOTD);
    const auto files = j.find("files");
    if (files != j.end() && files->is_array())
        for (const json& e : *files)
            add_item(c, e, base_url, "", seen);
    const auto dirs = j.find("directories");
    if (dirs != j.end() && dirs->is_array())
        for (const json& d : *dirs) {
            std::string ref = d.is_string() ? d.get<std::string>() : d.is_object() ? str_at(d, "url") : std::string();
            const std::string url = ref.size() <= MAX_URL ? resolve(base_url, ref) : std::string();
            if (!url.empty() && c.directories.size() < MAX_DIRECTORIES &&
                !std::ranges::contains(c.directories, url))
                c.directories.push_back(url);
        }
    const auto tdb = j.find("titledb");
    if (tdb != j.end() && tdb->is_object())
        for (auto it = tdb->begin(); it != tdb->end() && c.titles.size() < MAX_ITEMS; ++it) {
            uint64_t id;
            if (!hex_u64(it.key(), &id) || !id || !it->is_object()) continue;
            TitleInfo t;
            t.name = display_text(str_at(*it, "name"), MAX_NAME);
            version_at(*it, &t.version);
            c.titles[id] = t;
        }
    // A titledb names the items it knows that have no name of their own but
    // their file's.
    for (Item& item : c.items) {
        const auto t = c.titles.find(base_of(item.title_id));
        if (t != c.titles.end() && !t->second.name.empty() && item.name == file_name(item.url)) item.name = t->second.name;
    }
    return c;
}

}   // namespace library
