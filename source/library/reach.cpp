// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/reach.hpp"
#include "library/atlas.hpp"
#include "library/signals.hpp"

#include <algorithm>

#include <aurore/extern/nlohmann/json.hpp>

namespace library
{

bool ReachBook::from_json(const std::string& text)
{
    this->games.clear();
    this->langs.clear();
    this->day.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != ATLAS_SCHEMA || !root.contains("games") || !root["games"].is_object() ||
        !root.contains("languages") || !root["languages"].is_array())
        return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    for (const auto& l : root["languages"]) {
        if (this->langs.size() >= MAX_LANGUAGES) break;
        // A name that is no language keeps its place (the shares go by position).
        this->langs.push_back(l.is_string() && l.get<std::string>().size() <= 8 ? l.get<std::string>() : std::string());
    }
    for (const auto& [key, value] : root["games"].items()) {
        if (this->games.size() >= MAX_GAMES) break;
        const uint64_t id = base_id_of(key);
        if (!id || !value.is_array()) continue;
        std::vector<uint8_t> shares(this->langs.size(), 0);
        for (size_t i = 0; i < shares.size() && i < value.size(); i++)
            if (value[i].is_number_integer()) shares[i] = (uint8_t)std::clamp(value[i].get<int>(), 0, 100);
        this->games[id] = std::move(shares);
    }
    this->day = generated->get<std::string>();
    return true;
}

int ReachBook::share(uint64_t base_id, const std::string& language) const
{
    if (language.empty()) return 0;
    const auto it = this->games.find(base_id);
    const auto l = std::ranges::find(this->langs, language);
    if (it == this->games.end() || l == this->langs.end()) return 0;
    return it->second[(size_t)(l - this->langs.begin())];
}

std::string language_of(const std::string& locale)
{
    std::string out = locale.substr(0, locale.find_first_of("-_"));
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

}   // namespace library
