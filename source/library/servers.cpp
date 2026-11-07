// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/servers.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <random>
#include <sys/stat.h>

#include <aurore/extern/nlohmann/json.hpp>

#include "util/paths.hpp"

namespace library
{
namespace
{
using json = nlohmann::json;

// servers.json as last read, with its size and time: the Home tab asks for
// the servers at every refresh, and a stat costs less than reading and
// parsing it again. save_servers() forgets it.
struct Cache
{
    std::string         path;
    off_t               size  = -1;
    time_t              mtime = 0;
    std::vector<Server> servers;
};
std::mutex s_cache_lock;
Cache      s_cache;

const char* connection_name(Connection c) { return c == Connection::Lan ? "lan" : c == Connection::Wan ? "wan" : "auto"; }

std::string str(const json& j, const char* key, size_t max = 2048)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return "";
    std::string s = it->get<std::string>();
    return s.size() <= max ? s : "";
}

bool https_or_empty(const std::string& u) { return u.empty() || (u.size() > 8 && u.starts_with("https://")); }

// The keys of a server servers_to_json writes.
constexpr const char* KNOWN[] = { "id", "name", "lan_url", "wan_url", "connection", "uid" };

// The schema of servers.json's text: 1 when it has none (files from before
// it was read), 0 when it is not a whole number.
int64_t schema_of(const json& root)
{
    const auto it = root.find("schema");
    if (it == root.end()) return 1;
    if (it->is_number_unsigned()) return it->get<uint64_t>() > INT32_MAX ? INT32_MAX : (int64_t)it->get<uint64_t>();
    return it->is_number_integer() ? it->get<int64_t>() : 0;
}

json kept_of(const std::string& kept)
{
    json j = kept.empty() ? json::object() : json::parse(kept, nullptr, false);
    return j.is_object() ? j : json::object();
}
}   // namespace

std::string servers_to_json(const std::vector<Server>& servers, const std::string& previous)
{
    json list = json::array();
    for (const Server& s : servers) {
        // What this build does not know first.
        json j = kept_of(s.kept);
        j["id"] = s.id;
        j["name"] = s.name;
        j["lan_url"] = s.lan_url;
        j["wan_url"] = s.wan_url;
        j["connection"] = connection_name(s.connection);
        j["uid"] = s.uid;
        list.push_back(j);
    }
    json root = previous.empty() ? json::object() : json::parse(previous, nullptr, false);
    if (!root.is_object()) root = json::object();
    root["schema"] = SERVERS_SCHEMA;
    root["servers"] = list;
    return root.dump(2) + "\n";
}

// Servers of schema 1 or 2: the same keys, apart from what schema 1 held
// for the other protocols (a user name, a sealed password, a pin, a format),
// which are not read. A schema 1 server has no `uid`: it pairs again.
std::vector<Server> servers_from_json(const std::string& text)
{
    std::vector<Server> out;
    const json          root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return out;
    const int64_t schema = schema_of(root);
    if (schema != SERVERS_SCHEMA && schema != OLD_SERVERS_SCHEMA) return out;
    const auto list = root.find("servers");
    if (list == root.end() || !list->is_array()) return out;
    for (const json& j : *list) {
        if (!j.is_object() || out.size() >= MAX_SERVERS) continue;
        Server s;
        s.id = str(j, "id", 64);
        s.name = str(j, "name", 128);
        s.lan_url = str(j, "lan_url");
        s.wan_url = str(j, "wan_url");
        const std::string c = str(j, "connection", 8);
        s.connection = c == "lan" ? Connection::Lan : c == "wan" ? Connection::Wan : Connection::Auto;
        if (schema == SERVERS_SCHEMA) s.uid = str(j, "uid", 64);
        // An address that is not https (a file edited by hand) is dropped.
        if (!https_or_empty(s.lan_url)) s.lan_url.clear();
        if (!https_or_empty(s.wan_url)) s.wan_url.clear();
        json kept = json::object();
        for (const auto& [key, value] : j.items()) {
            if (schema == OLD_SERVERS_SCHEMA && (key == "format" || key == "username" || key == "password" || key == "pin")) continue;
            if (std::ranges::find_if(KNOWN, [&](const char* k) { return key == k; }) == std::end(KNOWN)) kept[key] = value;
        }
        if (!kept.empty()) s.kept = kept.dump();
        if (s.id.empty() || (s.lan_url.empty() && s.wan_url.empty())) continue;
        out.push_back(std::move(s));
    }
    return out;
}

bool servers_old(const std::string& text)
{
    const json root = json::parse(text, nullptr, false);
    return root.is_object() && root.contains("servers") && schema_of(root) == OLD_SERVERS_SCHEMA;
}

std::string servers_file() { return paths::data_dir() + "/servers.json"; }

std::vector<Server> load_servers()
{
    const std::string file = servers_file();
    struct stat       st{};
    if (::stat(file.c_str(), &st) != 0) return {};
    {
        std::lock_guard lock(s_cache_lock);
        if (s_cache.path == file && s_cache.size == st.st_size && s_cache.mtime == st.st_mtime) return s_cache.servers;
    }
    std::string text;
    if (!paths::read_file(file, text)) return {};
    std::vector<Server> servers = servers_from_json(text);
    // Once: servers.json of schema 1 is written again as schema 2, without
    // the credentials of the other protocols (Remove with OLD_SERVERS_SCHEMA).
    if (servers_old(text)) {
        std::string why;
        if (save_servers(servers, &why)) return load_servers();
    }
    std::lock_guard lock(s_cache_lock);
    s_cache = { file, st.st_size, st.st_mtime, servers };
    return servers;
}

bool servers_newer(const std::string& text)
{
    const json root = json::parse(text, nullptr, false);
    return root.is_object() && schema_of(root) > SERVERS_SCHEMA;
}

bool save_servers(const std::vector<Server>& servers, std::string* why)
{
    // A newer build's file is left as it is: this one would lose what it
    // cannot read.
    std::string old;
    if (paths::read_file(servers_file(), old) && servers_newer(old)) {
        if (why) *why = "servers.json is from a newer PlayShelf";
        return false;
    }
    if (!paths::ensure_dir(paths::data_dir())) {
        if (why) *why = "cannot create " + paths::data_dir();
        return false;
    }
    const bool written = paths::atomic_write(servers_file(), servers_to_json(servers, old), why);
    // After the write, so that a load meanwhile does not keep the old file.
    std::lock_guard lock(s_cache_lock);
    s_cache = {};
    return written;
}

std::string new_server_id()
{
    static const char  HEX[] = "0123456789abcdef";
    std::random_device rd;
    std::string        id;
    for (int i = 0; i < 16; i++)
        id += HEX[rd() & 15];
    return id;
}

const Server* find_server(const std::vector<Server>& servers, const std::string& id)
{
    for (const Server& s : servers)
        if (s.id == id) return &s;
    return nullptr;
}

std::string check_server(const Server& s)
{
    if (s.lan_url.empty() && s.wan_url.empty()) return "an address at least";
    for (const std::string* u : {&s.lan_url, &s.wan_url})
        if (!u->empty() && (u->size() <= 8 || u->compare(0, 8, "https://") != 0)) return "https:// addresses only";
    return "";
}

}   // namespace library
