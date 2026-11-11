// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/also.hpp"
#include "library/atlas.hpp"
#include "library/signals.hpp"

#include <aurore/extern/nlohmann/json.hpp>

namespace library
{

bool AlsoBook::from_json(const std::string& text)
{
    this->games.clear();
    this->day.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != ATLAS_SCHEMA || !root.contains("games") || !root["games"].is_object())
        return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    static const struct
    {
        const char* key;
        Store       store;
    } KEYS[] = { { "steam", STEAM }, { "ps", PLAYSTATION }, { "xbox", XBOX }, { "gog", GOG }, { "epic", EPIC } };
    for (const auto& [key, value] : root["games"].items()) {
        if (this->games.size() >= MAX_GAMES) break;
        const uint64_t id = base_id_of(key);
        if (!id || !value.is_object()) continue;
        uint8_t stores = 0;
        // A store counts when it lists at least one id; what it does not know is skipped.
        for (const auto& k : KEYS) {
            const auto it = value.find(k.key);
            if (it != value.end() && it->is_array() && !it->empty()) stores |= k.store;
        }
        if (stores) this->games[id] = stores;
    }
    this->day = generated->get<std::string>();
    return true;
}

uint8_t AlsoBook::of(uint64_t base_id) const
{
    const auto it = this->games.find(base_id);
    return it == this->games.end() ? 0 : it->second;
}

std::string store_names(uint8_t stores)
{
    static const struct
    {
        Store       store;
        const char* name;
    } NAMES[] = { { STEAM, "Steam" }, { PLAYSTATION, "PlayStation" }, { XBOX, "Xbox" }, { GOG, "GOG" }, { EPIC, "Epic Games" } };
    std::string out;
    for (const auto& n : NAMES)
        if (stores & n.store) out += (out.empty() ? "" : ", ") + std::string(n.name);
    return out;
}

}   // namespace library
