// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/popular.hpp"

#include <algorithm>
#include <cstdlib>

#include <borealis/extern/nlohmann/json.hpp>

namespace library
{

namespace
{
bool is_day(const std::string& s)
{
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i = 0; i < s.size(); i++)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
    return true;
}

// A base game's title id, as 16 hex digits; 0 for anything else.
uint64_t id_of(const std::string& s)
{
    if (s.size() != 16) return 0;
    uint64_t id = 0;
    for (char c : s) {
        int v = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (v < 0) return 0;
        id = id << 4 | (uint64_t)v;
    }
    // A base game's: ends in 000, bit 12 clear.
    return (id & 0xFFF) == 0 && !(id & 0x1000) ? id : 0;
}
}   // namespace

bool PopularBook::from_json(const std::string& text)
{
    this->games.clear();
    this->day.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || !root.contains("games") || !root["games"].is_object()) return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    for (const auto& [key, value] : root["games"].items()) {
        if (this->games.size() >= MAX_GAMES) break;
        const uint64_t id = id_of(key);
        if (!id || !value.is_array() || value.size() < 2 || !value[0].is_number_integer() || !value[1].is_number_integer())
            continue;
        Renown r;
        r.score = std::clamp(value[0].get<int>(), 0, 100);
        r.trend = std::clamp(value[1].get<int>(), -100, 999);
        // A third field (PlayInsights' schema 1): the critics' score.
        if (value.size() >= 3 && value[2].is_number_integer()) r.critics = std::clamp(value[2].get<int>(), 0, 100);
        if (r.score > 0) this->games[id] = r;
    }
    this->day = generated->get<std::string>();
    return true;
}

Renown PopularBook::of(uint64_t base_id) const
{
    const auto it = this->games.find(base_id);
    return it == this->games.end() ? Renown{} : it->second;
}

}   // namespace library
