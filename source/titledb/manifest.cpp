// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "titledb/manifest.hpp"

#include <cctype>

#include "util/json_util.hpp"

namespace titledb
{
namespace
{
constexpr uint64_t MAX_PACK = 2ull << 30;   // a pack's size at most

// The first of `keys` that holds a string.
std::string first_string(const nlohmann::json& j, std::initializer_list<const char*> keys)
{
    for (const char* k : keys) {
        const std::string v = json_util::string_at(j, k);
        if (!v.empty()) return v;
    }
    return "";
}

const nlohmann::json* first_object(const nlohmann::json& j, std::initializer_list<const char*> keys)
{
    for (const char* k : keys)
        if (const nlohmann::json* o = json_util::object_at(j, k)) return o;
    return nullptr;
}

bool hex64(std::string* s)
{
    if (s->size() != 64) return false;
    for (char& c : *s) {
        if (!std::isxdigit((unsigned char)c)) return false;
        c = (char)std::tolower((unsigned char)c);
    }
    return true;
}

bool pack(const nlohmann::json& j, const std::string& base, const char* name, PackFile* out, std::string* why)
{
    out->url = resolve(base, first_string(j, { "url", "path" }));
    out->sha256 = first_string(j, { "sha256", "sha_256" });
    int64_t size = 0;
    json_util::integer_at(j, "size", &size);
    if (!out->url.starts_with("https://")) {
        if (why) *why = std::string(name) + ": not an HTTPS address";
        return false;
    }
    if (!hex64(&out->sha256)) {
        if (why) *why = std::string(name) + ": no SHA-256";
        return false;
    }
    if (size <= 0 || (uint64_t)size > MAX_PACK) {
        if (why) *why = std::string(name) + ": no size";
        return false;
    }
    out->size = (uint64_t)size;
    return true;
}
}   // namespace

std::string resolve(const std::string& base, const std::string& url)
{
    if (url.empty() || url.contains("://")) return url;
    const size_t scheme = base.find("://");
    if (scheme == std::string::npos) return "";
    if (url[0] == '/') {
        const size_t path = base.find('/', scheme + 3);
        return (path == std::string::npos ? base : base.substr(0, path)) + url;
    }
    const size_t query = base.find_first_of("?#");
    const std::string b = base.substr(0, query);
    const size_t slash = b.rfind('/');
    return (slash > scheme + 2 ? b.substr(0, slash + 1) : b + "/") + url;
}

bool parse_manifest(const std::string& text, const std::string& manifest_url, Manifest* out, std::string* why)
{
    nlohmann::json j;
    if (!json_util::parse(text, &j) || !j.is_object()) {
        if (why) *why = "not JSON";
        return false;
    }
    *out = {};
    out->version = first_string(j, { "db_version", "version" });
    if (out->version.empty()) {
        if (int64_t v = 0; json_util::integer_at(j, "db_version", &v) || json_util::integer_at(j, "version", &v))
            out->version = std::to_string(v);
    }
    if (out->version.empty() || out->version.size() > 64) {
        if (why) *why = "no version";
        return false;
    }
    const nlohmann::json* files = json_util::object_at(j, "files");
    const nlohmann::json& in = files ? *files : j;
    const nlohmann::json* titles = first_object(in, { "titles.pack", "titles_pack", "titles" });
    if (!titles) {
        if (why) *why = "no titles pack";
        return false;
    }
    if (!pack(*titles, manifest_url, "titles", &out->titles, why)) return false;
    if (const nlohmann::json* icons = first_object(in, { "icons.pack", "icons_pack", "icons" }))
        if (!pack(*icons, manifest_url, "icons", &out->icons, why)) return false;
    return true;
}

}   // namespace titledb
