// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/peer_book.hpp"

#include <algorithm>
#include <cstdio>

#include <aurore/extern/nlohmann/json.hpp>

#include "util/paths.hpp"

namespace lantern_app
{

namespace
{
constexpr int SCHEMA = 1;

bool hex64(const std::string& s)
{
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
}   // namespace

bool PeerBook::load(const std::string& path)
{
    this->uid_.clear();
    this->servers_.clear();
    this->held_.clear();
    this->extra_.clear();
    this->newer = false;
    std::string text;
    if (!paths::read_file(path, text)) return true;   // none yet
    const auto doc = nlohmann::json::parse(text, nullptr, false);
    if (!doc.is_object() || !doc.contains("schema") || !doc["schema"].is_number_integer()) return false;
    if (doc["schema"].get<int>() > SCHEMA) {
        this->newer = true;
        return true;
    }
    nlohmann::json extra = nlohmann::json::object();
    for (auto it = doc.begin(); it != doc.end(); ++it) {
        const std::string& k = it.key();
        if (k != "schema" && k != "uid" && k != "servers" && k != "held") extra[k] = it.value();
    }
    this->extra_ = extra.dump();
    this->uid_   = doc.value("uid", std::string());
    if (doc.contains("servers") && doc["servers"].is_object())
        for (auto it = doc["servers"].begin(); it != doc["servers"].end(); ++it)
            if (it.value().is_string() && hex64(it.value().get<std::string>()) && this->servers_.size() < MAX_SERVERS)
                this->servers_[it.key()] = it.value().get<std::string>();
    if (doc.contains("held") && doc["held"].is_array())
        for (const auto& h : doc["held"]) {
            if (!h.is_object() || this->held_.size() >= MAX_HELD) continue;
            HeldFile f;
            f.sha256 = h.value("sha256", std::string());
            f.size   = h.value("size", uint64_t(0));
            f.path   = h.value("path", std::string());
            if (hex64(f.sha256) && !f.path.empty() && f.path[0] == '/') this->held_.push_back(std::move(f));
        }
    return true;
}

bool PeerBook::save(const std::string& path) const
{
    if (this->newer) return false;   // never over a newer schema
    nlohmann::json doc = nlohmann::json::parse(this->extra_.empty() ? "{}" : this->extra_, nullptr, false);
    if (!doc.is_object()) doc = nlohmann::json::object();
    doc["schema"]  = SCHEMA;
    doc["uid"]     = this->uid_;
    doc["servers"] = this->servers_;
    nlohmann::json held = nlohmann::json::array();
    for (const HeldFile& f : this->held_) held.push_back({ { "sha256", f.sha256 }, { "size", f.size }, { "path", f.path } });
    doc["held"] = held;
    return paths::atomic_write(path, doc.dump(1));
}

void PeerBook::set_server(const std::string& server_uid, const std::string& key_hex)
{
    if (!hex64(key_hex) || server_uid.empty()) return;
    if (this->servers_.size() < MAX_SERVERS || this->servers_.count(server_uid)) this->servers_[server_uid] = key_hex;
}

void PeerBook::hold(const HeldFile& file)
{
    if (!hex64(file.sha256) || file.path.empty() || file.path[0] != '/') return;
    this->release(file.path);
    if (this->held_.size() < MAX_HELD) this->held_.push_back(file);
}

void PeerBook::release(const std::string& path)
{
    this->held_.erase(std::remove_if(this->held_.begin(), this->held_.end(), [&](const HeldFile& f) { return f.path == path; }),
                      this->held_.end());
}

const HeldFile* PeerBook::find(const std::string& sha256) const
{
    for (const HeldFile& f : this->held_)
        if (f.sha256 == sha256) return &f;
    return nullptr;
}

std::string uuid_from(const uint8_t b[16])
{
    uint8_t v[16];
    std::copy(b, b + 16, v);
    v[6] = (uint8_t)((v[6] & 0x0f) | 0x40);
    v[8] = (uint8_t)((v[8] & 0x3f) | 0x80);
    char out[37];
    std::snprintf(out, sizeof out, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", v[0], v[1], v[2],
                  v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
    return out;
}

}   // namespace lantern_app
