// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/seen.hpp"

#include <cinttypes>
#include <cstdio>
#include <ctime>

#include <aurore/extern/nlohmann/json.hpp>

namespace library
{

namespace
{
constexpr int SCHEMA = 1;

std::string hex(uint64_t id)
{
    char out[17];
    std::snprintf(out, sizeof(out), "%016" PRIX64, id);
    return out;
}

uint64_t parse_hex(const std::string& s)
{
    if (s.size() != 16) return 0;
    uint64_t id = 0;
    for (char c : s) {
        int v = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (v < 0) return 0;
        id = id << 4 | (uint64_t)v;
    }
    return id;
}

bool is_date(uint32_t d)
{
    return d >= 20000101 && d <= 29991231 && d % 100 >= 1 && d % 100 <= 31 && d / 100 % 100 >= 1 && d / 100 % 100 <= 12;
}

// Days since 1970-01-01 of a YYYYMMDD date (Howard Hinnant's days_from_civil).
long day_number(uint32_t d)
{
    long       y = (long)(d / 10000);
    const long m = (long)(d / 100 % 100), day = (long)(d % 100);
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
}   // namespace

long days_between(uint32_t a, uint32_t b)
{
    if (!is_date(a) || !is_date(b)) return 0;
    return day_number(b) - day_number(a);
}

bool SeenBook::note(const std::string& server_id, const std::vector<uint64_t>& ids, uint32_t day)
{
    if (server_id.empty() || !is_date(day)) return false;
    auto it = this->servers.find(server_id);
    bool changed = false;
    if (it == this->servers.end()) {
        if (this->servers.size() >= MAX_SERVERS) return false;
        // The first reading: all there from the start.
        Server s;
        for (uint64_t id : ids) {
            if (s.known.size() >= MAX_GAMES) break;
            if (id) s.known.insert(id);
        }
        this->servers.emplace(server_id, std::move(s));
        return true;
    }
    Server& s = it->second;
    for (uint64_t id : ids) {
        if (!id || s.known.contains(id)) continue;
        if (s.known.size() >= MAX_GAMES) break;
        s.known.insert(id);
        s.dated[id] = day;
        changed = true;
    }
    // Dates too old to say "new": only known.
    for (auto d = s.dated.begin(); d != s.dated.end();) {
        if (days_between(d->second, day) > (long)MAX_DAYS) {
            d = s.dated.erase(d);
            changed = true;
        } else {
            ++d;
        }
    }
    return changed;
}

uint32_t SeenBook::first_seen(const std::string& server_id, uint64_t id) const
{
    const auto it = this->servers.find(server_id);
    if (it == this->servers.end()) return 0;
    const auto d = it->second.dated.find(id);
    return d == it->second.dated.end() ? 0 : d->second;
}

bool SeenBook::forget(const std::string& server_id)
{
    return this->servers.erase(server_id) != 0;
}

std::string SeenBook::to_json() const
{
    nlohmann::json out = nlohmann::json::object();
    for (const auto& [id, s] : this->servers) {
        std::string known;
        known.reserve(s.known.size() * 17);
        for (uint64_t k : s.known) {
            if (!known.empty()) known += ',';
            known += hex(k);
        }
        nlohmann::json dated = nlohmann::json::object();
        for (const auto& [k, day] : s.dated) dated[hex(k)] = day;
        out[id] = { { "known", known }, { "dated", std::move(dated) } };
    }
    return nlohmann::json({ { "schema", SCHEMA }, { "servers", std::move(out) } }).dump();
}

bool SeenBook::from_json(const std::string& text)
{
    this->servers.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != SCHEMA || !root.contains("servers") || !root["servers"].is_object())
        return false;
    for (const auto& [id, j] : root["servers"].items()) {
        if (this->servers.size() >= MAX_SERVERS) break;
        if (id.empty() || !j.is_object()) continue;
        Server s;
        if (j.contains("known") && j["known"].is_string()) {
            const std::string& known = j["known"].get_ref<const std::string&>();
            for (size_t at = 0; at + 16 <= known.size() && s.known.size() < MAX_GAMES; at += 17)
                if (const uint64_t k = parse_hex(known.substr(at, 16))) s.known.insert(k);
        }
        if (j.contains("dated") && j["dated"].is_object())
            for (const auto& [k, day] : j["dated"].items()) {
                const uint64_t game = parse_hex(k);
                if (!game || !day.is_number_unsigned() || !is_date(day.get<uint32_t>()) || !s.known.contains(game)) continue;
                s.dated[game] = day.get<uint32_t>();
            }
        this->servers.emplace(id, std::move(s));
    }
    return true;
}

std::string ids_text(const std::vector<uint64_t>& ids)
{
    std::string out;
    for (uint64_t id : ids) out += hex(id) + "\n";
    return out;
}

std::vector<uint64_t> ids_of(const std::string& text, size_t max)
{
    std::vector<uint64_t> out;
    std::set<uint64_t>    once;
    size_t                start = 0;
    while (start < text.size() && out.size() < max) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const uint64_t id = parse_hex(line);
        if (id && once.insert(id).second) out.push_back(id);
        start = end + 1;
    }
    return out;
}

}   // namespace library
