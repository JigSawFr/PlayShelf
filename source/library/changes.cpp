// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/changes.hpp"
#include "library/atlas.hpp"

#include <aurore/extern/nlohmann/json.hpp>

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

// A trend's bounds: popular.json's, -100 to 999.
bool is_trend(const nlohmann::json& v)
{
    return v.is_number_integer() && v.get<int64_t>() >= -100 && v.get<int64_t>() <= 999;
}
}   // namespace

bool ChangeBook::from_json(const std::string& text)
{
    this->out.clear();
    this->fresh.clear();
    this->moved.clear();
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
    if (const auto n = sw->find("new"); n != sw->end() && n->is_array()) {
        std::unordered_set<uint64_t> once;
        for (const nlohmann::json& v : *n) {
            if (this->fresh.size() >= MAX_IDS) break;
            if (!v.is_string()) continue;
            if (const uint64_t id = base_id_of(v.get<std::string>()); id && once.insert(id).second) this->fresh.push_back(id);
        }
    }
    if (const auto t = sw->find("trends"); t != sw->end() && t->is_array()) {
        std::unordered_set<uint64_t> once;
        for (const nlohmann::json& v : *t) {
            if (this->moved.size() >= MAX_IDS) break;
            if (!v.is_array() || v.size() < 3 || !v[0].is_string() || !is_trend(v[1]) || !is_trend(v[2])) continue;
            const uint64_t id = base_id_of(v[0].get<std::string>());
            if (id && once.insert(id).second) this->moved.push_back({ id, { v[1].get<int>(), v[2].get<int>() } });
        }
    }
    return true;
}

uint32_t ChangeBook::day_number() const
{
    if (this->day.size() != 10) return 0;
    return (uint32_t)std::stoul(this->day.substr(0, 4) + this->day.substr(5, 2) + this->day.substr(8, 2));
}

size_t ChangeBook::size() const
{
    return this->out.size() + this->fresh.size() + this->moved.size();
}

}   // namespace library
