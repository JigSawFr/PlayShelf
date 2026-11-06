// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/credential_store.hpp"

#include <aurore/extern/nlohmann/json.hpp>

#include "util/paths.hpp"
#include "util/secret_box.hpp"

namespace lantern_app
{
namespace
{
using json = nlohmann::json;

constexpr char HEX[] = "0123456789abcdef";

std::string to_hex(const lantern::KeySeed& seed)
{
    std::string out;
    for (const uint8_t b : seed) {
        out += HEX[b >> 4];
        out += HEX[b & 15];
    }
    return out;
}

int nibble(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

std::optional<lantern::KeySeed> seed_of(const std::string& hex)
{
    if (hex.size() != 64) return std::nullopt;
    lantern::KeySeed seed{};
    for (size_t i = 0; i < 32; i++) {
        const int hi = nibble(hex[2 * i]), lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        seed[i] = (uint8_t)(hi << 4 | lo);
    }
    return seed;
}

// The file's object, or an empty one when it is absent or unreadable.
// `newer` tells a file of a later schema, which is never written over.
json read_root(const std::string& path, bool& newer)
{
    newer = false;
    std::string text;
    if (!paths::read_file(path, text)) return json::object();
    json root = json::parse(text, nullptr, false);
    if (!root.is_object()) return json::object();
    const auto s = root.find("schema");
    if (s != root.end() && (!s->is_number_integer() || s->get<int64_t>() != CREDENTIALS_SCHEMA)) {
        newer = !s->is_number_integer() || s->get<int64_t>() > CREDENTIALS_SCHEMA;
        return json::object();
    }
    return root;
}

lantern::Result<void> write_root(const std::string& path, json root)
{
    root["schema"] = CREDENTIALS_SCHEMA;
    if (!paths::ensure_dir(paths::data_dir())) return lantern::fail(lantern::Category::storage, "cannot create the data folder");
    std::string why;
    if (!paths::atomic_write(path, root.dump(2) + "\n", &why)) return lantern::fail(lantern::Category::storage, why);
    return {};
}
}   // namespace

std::string credentials_file() { return paths::data_dir() + "/lantern.json"; }

lantern::Result<std::optional<lantern::Credentials>> FileCredentialStore::load(std::string_view uid, lantern::Allocator allocator)
{
    bool       newer = false;
    const json root  = read_root(path_, newer);
    const auto all   = root.find("servers");
    if (all == root.end() || !all->is_object()) return std::optional<lantern::Credentials>{};
    const auto it = all->find(std::string(uid));
    if (it == all->end() || !it->is_object()) return std::optional<lantern::Credentials>{};

    // A token this console cannot open (another console's, or changed) is
    // no token: the server is paired again.
    const auto sealed = it->find("token");
    if (sealed == it->end() || !sealed->is_string()) return std::optional<lantern::Credentials>{};
    const auto token = secret_box::open(sealed->get<std::string>());
    if (!token || token->empty()) return std::optional<lantern::Credentials>{};

    lantern::Credentials c{allocator};
    c.token.assign(*token);
    if (const auto k = it->find("key"); k != it->end() && k->is_string()) {
        const auto seed = secret_box::open(k->get<std::string>());
        if (!seed) return std::optional<lantern::Credentials>{};
        c.key = seed_of(*seed);
        if (!c.key) return std::optional<lantern::Credentials>{};
    }
    if (const auto p = it->find("spki"); p != it->end() && p->is_string()) {
        c.spki = lantern::digest_from_hex(p->get<std::string>());
        if (!c.spki) return std::optional<lantern::Credentials>{};
    }
    return std::optional<lantern::Credentials>{std::move(c)};
}

lantern::Result<void> FileCredentialStore::store(std::string_view uid, const lantern::Credentials& credentials)
{
    bool  newer = false;
    json  root  = read_root(path_, newer);
    if (newer) return lantern::fail(lantern::Category::storage, "lantern.json is from a newer PlayShelf");
    const auto token = secret_box::seal(std::string(credentials.token));
    if (!token) return lantern::fail(lantern::Category::storage, "cannot seal the token");

    json entry = json::object();
    // What this build does not know of the entry stays.
    if (root.contains("servers") && root["servers"].is_object()) {
        const auto old = root["servers"].find(std::string(uid));
        if (old != root["servers"].end() && old->is_object()) entry = *old;
    }
    entry["token"] = *token;
    entry.erase("key");
    entry.erase("spki");
    if (credentials.key) {
        const auto key = secret_box::seal(to_hex(*credentials.key));
        if (!key) return lantern::fail(lantern::Category::storage, "cannot seal the device key");
        entry["key"] = *key;
    }
    if (credentials.spki) {
        const auto hex = lantern::to_hex(*credentials.spki);
        entry["spki"]  = std::string(hex.data(), hex.size());
    }
    if (!root.contains("servers") || !root["servers"].is_object()) root["servers"] = json::object();
    root["servers"][std::string(uid)] = entry;
    return write_root(path_, std::move(root));
}

lantern::Result<void> FileCredentialStore::erase(std::string_view uid)
{
    bool newer = false;
    json root  = read_root(path_, newer);
    if (newer) return lantern::fail(lantern::Category::storage, "lantern.json is from a newer PlayShelf");
    if (!root.contains("servers") || !root["servers"].is_object()) return {};
    if (root["servers"].erase(std::string(uid)) == 0) return {};
    return write_root(path_, std::move(root));
}

}   // namespace lantern_app
