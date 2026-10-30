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
#include "util/secret_box.hpp"

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
const char* format_name(Format f) { return f == Format::Api ? "api" : f == Format::Index ? "index" : "auto"; }

std::string str(const json& j, const char* key, size_t max = 2048)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return "";
    std::string s = it->get<std::string>();
    return s.size() <= max ? s : "";
}

bool https_or_empty(const std::string& u) { return u.empty() || (u.size() > 8 && u.starts_with("https://")); }

// The keys of a server servers_to_json writes.
constexpr const char* KNOWN[] = { "id", "name", "lan_url", "wan_url", "connection", "format", "username", "password", "pin" };

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
        // What this build does not know first; a password or pin it holds
        // replaces one it could not read.
        json j = kept_of(s.kept);
        if (!s.password_box.empty()) j.erase("password");
        if (!s.pin.empty()) j.erase("pin");
        j["id"] = s.id;
        j["name"] = s.name;
        j["lan_url"] = s.lan_url;
        j["wan_url"] = s.wan_url;
        j["connection"] = connection_name(s.connection);
        j["format"] = format_name(s.format);
        j["username"] = s.username;
        if (!j.contains("password")) j["password"] = s.password_box;
        if (!j.contains("pin")) j["pin"] = s.pin;
        list.push_back(j);
    }
    json root = previous.empty() ? json::object() : json::parse(previous, nullptr, false);
    if (!root.is_object()) root = json::object();
    root["schema"] = SERVERS_SCHEMA;
    root["servers"] = list;
    return root.dump(2) + "\n";
}

std::vector<Server> servers_from_json(const std::string& text)
{
    std::vector<Server> out;
    const json          root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object() || schema_of(root) != SERVERS_SCHEMA) return out;
    const auto list = root.find("servers");
    if (list == root.end() || !list->is_array()) return out;
    for (const json& j : *list) {
        if (!j.is_object() || out.size() >= MAX_SERVERS) continue;
        Server s;
        s.id = str(j, "id", 64);
        s.name = str(j, "name", 128);
        s.lan_url = str(j, "lan_url");
        s.wan_url = str(j, "wan_url");
        const std::string c = str(j, "connection", 8), f = str(j, "format", 8);
        s.connection = c == "lan" ? Connection::Lan : c == "wan" ? Connection::Wan : Connection::Auto;
        s.format = f == "api" ? Format::Api : f == "index" ? Format::Index : Format::Auto;
        s.username = str(j, "username", 256);
        s.password_box = str(j, "password", 4096);
        s.pin = str(j, "pin", 128);
        // An address that is not https (a file edited by hand) is dropped.
        // A password or pin of a format this build cannot read (a newer
        // build's, or a password typed in as it is) is never used, and
        // kept in the file as it is.
        if (!https_or_empty(s.lan_url)) s.lan_url.clear();
        if (!https_or_empty(s.wan_url)) s.wan_url.clear();
        json kept = json::object();
        for (const auto& [key, value] : j.items())
            if (std::ranges::find_if(KNOWN, [&](const char* k) { return key == k; }) == std::end(KNOWN)) kept[key] = value;
        if (!s.password_box.empty() && !secret_box::is_sealed(s.password_box)) {
            kept["password"] = j.at("password");
            s.password_box.clear();
        }
        if (!s.pin.empty() && !s.pin.starts_with("sha256//")) {
            kept["pin"] = j.at("pin");
            s.pin.clear();
        }
        if (!kept.empty()) s.kept = kept.dump();
        if (s.id.empty() || (s.lan_url.empty() && s.wan_url.empty())) continue;
        out.push_back(std::move(s));
    }
    return out;
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
    std::lock_guard     lock(s_cache_lock);
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

std::string password_of(const Server& s)
{
    if (s.password_box.empty()) return "";
    const auto p = secret_box::open(s.password_box);
    return p ? *p : "";
}

bool set_password(Server& s, const std::string& password)
{
    if (password.empty()) {
        s.password_box.clear();
        // One that could not be read goes too: the user cleared it.
        json kept = kept_of(s.kept);
        kept.erase("password");
        s.kept = kept.empty() ? "" : kept.dump();
        return true;
    }
    const auto box = secret_box::seal(password);
    if (!box) return false;
    s.password_box = *box;
    return true;
}

std::string pin_shown(const Server& s)
{
    const std::string prefix = "sha256//";
    if (!s.pin.starts_with(prefix)) return "";
    const auto raw = unbase64(s.pin.substr(prefix.size()));
    if (!raw || raw->size() != 32) return "";
    std::array<uint8_t, 32> h{};
    std::copy(raw->begin(), raw->end(), h.begin());
    return pin_short(h);
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
