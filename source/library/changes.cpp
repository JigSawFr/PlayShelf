// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/changes.hpp"
#include "library/atlas.hpp"

#include <aurore/extern/nlohmann/json.hpp>

#include "library/prices.hpp"
#include "library/signals.hpp"

namespace library
{

namespace
{
// The base games' ids of a list of them; nothing of anything else.
void ids_of(const nlohmann::json& list, std::unordered_set<uint64_t>& into)
{
    if (!list.is_array()) return;
    for (const nlohmann::json& v : list) {
        if (into.size() >= ChangeBook::MAX_IDS) return;
        if (!v.is_string()) continue;
        if (const uint64_t id = base_id_of(v.get<std::string>())) into.insert(id);
    }
}
}   // namespace

bool ChangeBook::from_json(const std::string& text)
{
    this->out.clear();
    this->sales.clear();
    this->day.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != ATLAS_SCHEMA) return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    this->day = generated->get<std::string>();
    const auto platforms = root.find("platforms");
    if (platforms == root.end() || !platforms->is_object()) return true;
    const auto sw = platforms->find("switch");
    if (sw == platforms->end() || !sw->is_object()) return true;
    if (const auto o = sw->find("out_today"); o != sw->end()) ids_of(*o, this->out);
    const auto by_country = sw->find("sales");
    if (by_country == sw->end() || !by_country->is_object()) return true;
    for (const auto& [country, kinds] : by_country->items()) {
        if (!is_price_country(country) || !kinds.is_object()) continue;
        Sales& s = this->sales[country];
        if (const auto k = kinds.find("started"); k != kinds.end()) ids_of(*k, s.started);
        if (const auto k = kinds.find("lowest"); k != kinds.end()) ids_of(*k, s.lowest);
    }
    return true;
}

uint32_t ChangeBook::day_number() const
{
    if (this->day.size() != 10) return 0;
    return (uint32_t)std::stoul(this->day.substr(0, 4) + this->day.substr(5, 2) + this->day.substr(8, 2));
}

bool ChangeBook::sale_started(const std::string& country, uint64_t id) const
{
    const auto it = this->sales.find(country);
    return it != this->sales.end() && it->second.started.contains(id);
}

bool ChangeBook::new_lowest(const std::string& country, uint64_t id) const
{
    const auto it = this->sales.find(country);
    return it != this->sales.end() && it->second.lowest.contains(id);
}

size_t ChangeBook::size() const
{
    size_t n = this->out.size();
    for (const auto& [_, s] : this->sales) n += s.started.size() + s.lowest.size();
    return n;
}

}   // namespace library
