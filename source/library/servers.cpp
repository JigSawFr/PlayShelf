// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/servers.hpp"

#include <algorithm>
#include <array>
#include <random>

#include <aurore/extern/nlohmann/json.hpp>

#include "util/paths.hpp"
#include "util/secret_box.hpp"

namespace library
{
namespace
{
using json = nlohmann::json;

const char* connection_name(Connection c) { return c == Connection::Lan ? "lan" : c == Connection::Wan ? "wan" : "auto"; }
const char* format_name(Format f) { return f == Format::Api ? "api" : f == Format::Index ? "index" : "auto"; }

std::string str(const json& j, const char* key, size_t max = 2048)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return "";
    std::string s = it->get<std::string>();
    return s.size() <= max ? s : "";
}

bool https_or_empty(const std::string& u) { return u.empty() || (u.size() > 8 && u.compare(0, 8, "https://") == 0); }
}   // namespace

std::string servers_to_json(const std::vector<Server>& servers)
{
    json list = json::array();
    for (const Server& s : servers) {
        json j;
        j["id"] = s.id;
        j["name"] = s.name;
        j["lan_url"] = s.lan_url;
        j["wan_url"] = s.wan_url;
        j["connection"] = connection_name(s.connection);
        j["format"] = format_name(s.format);
        j["username"] = s.username;
        j["password"] = s.password_box;
        j["pin"] = s.pin;
        list.push_back(j);
    }
    json root;
    root["schema"] = 1;
    root["servers"] = list;
    return root.dump(2) + "\n";
}

std::vector<Server> servers_from_json(const std::string& text)
{
    std::vector<Server> out;
    const json          root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return out;
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
        // An address that is not https (a file edited by hand) is dropped,
        // and a password kept as it is never used.
        if (!https_or_empty(s.lan_url)) s.lan_url.clear();
        if (!https_or_empty(s.wan_url)) s.wan_url.clear();
        if (!s.password_box.empty() && !secret_box::is_sealed(s.password_box)) s.password_box.clear();
        if (s.pin.compare(0, 8, "sha256//") != 0) s.pin.clear();
        if (s.id.empty() || (s.lan_url.empty() && s.wan_url.empty())) continue;
        out.push_back(std::move(s));
    }
    return out;
}

std::string servers_file() { return paths::data_dir() + "/servers.json"; }

std::vector<Server> load_servers()
{
    std::string text;
    if (!paths::read_file(servers_file(), text)) return {};
    return servers_from_json(text);
}

bool save_servers(const std::vector<Server>& servers, std::string* why)
{
    if (!paths::ensure_dir(paths::data_dir())) {
        if (why) *why = "cannot create " + paths::data_dir();
        return false;
    }
    return paths::atomic_write(servers_file(), servers_to_json(servers), why);
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
    if (s.pin.compare(0, prefix.size(), prefix) != 0) return "";
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
