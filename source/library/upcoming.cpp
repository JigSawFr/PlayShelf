// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/upcoming.hpp"
#include "library/atlas.hpp"
#include "library/signals.hpp"

#include <algorithm>

#include <aurore/extern/nlohmann/json.hpp>

namespace library
{

namespace
{
// YYYYMMDD with a month and a day that can be.
bool is_date(long v)
{
    const long month = v / 100 % 100, day = v % 100;
    return v >= 19700101 && v <= 29991231 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}
}   // namespace

bool UpcomingBook::from_json(const std::string& text)
{
    this->games.clear();
    this->day.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != ATLAS_SCHEMA || !root.contains("games") || !root["games"].is_object())
        return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    for (const auto& [key, value] : root["games"].items()) {
        if (this->games.size() >= MAX_GAMES) break;
        const uint64_t id = base_id_of(key);
        // [YYYYMMDD, name] or [YYYYMMDD, name, hypes]
        if (!id || !value.is_array() || value.size() < 2 || !value[0].is_number_integer() || !value[1].is_string()) continue;
        const long date = value[0].get<long>();
        if (!is_date(date)) continue;
        Upcoming u;
        u.date = (uint32_t)date;
        u.name = value[1].get<std::string>();
        if (u.name.empty() || u.name.size() > 200) continue;
        if (value.size() >= 3 && value[2].is_number_integer())
            u.hypes = (uint32_t)std::clamp(value[2].get<long>(), 0L, 100000000L);
        this->games[id] = std::move(u);
    }
    this->day = generated->get<std::string>();
    return true;
}

const Upcoming* UpcomingBook::find(uint64_t base_id) const
{
    const auto it = this->games.find(base_id);
    return it == this->games.end() ? nullptr : &it->second;
}

}   // namespace library
