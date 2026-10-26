// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/facts.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <set>

#include <borealis/extern/nlohmann/json.hpp>

namespace library
{
namespace
{
constexpr int SCHEMA = 1;

std::vector<std::string> strings_of(const nlohmann::json& j, const char* key, size_t max, size_t size)
{
    std::vector<std::string> out;
    const auto               it = j.find(key);
    if (it == j.end() || !it->is_array()) return out;
    for (const auto& v : *it)
        if (out.size() < max && v.is_string() && !v.get_ref<const std::string&>().empty() &&
            v.get_ref<const std::string&>().size() <= size)
            out.push_back(v.get<std::string>());
    return out;
}

std::string string_of(const nlohmann::json& j, const char* key, size_t size)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() && it->get_ref<const std::string&>().size() <= size ? it->get<std::string>() : "";
}

template <typename T>
T number_of(const nlohmann::json& j, const char* key, T most)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number_unsigned()) return 0;
    const uint64_t v = it->get<uint64_t>();
    return v > (uint64_t)most ? 0 : (T)v;
}
}   // namespace

const Facts* FactsBook::find(uint64_t base_id) const
{
    const auto it = this->games.find(base_id);
    return it == this->games.end() ? nullptr : &it->second;
}

bool FactsBook::put(uint64_t base_id, Facts facts)
{
    if (!base_id) return false;
    const auto it = this->games.find(base_id);
    if (it != this->games.end()) {
        it->second = std::move(facts);
        return true;
    }
    if (this->games.size() >= MAX_FACTS) return false;
    this->games.emplace(base_id, std::move(facts));
    return true;
}

void FactsBook::add_missing(const FactsBook& older)
{
    for (const auto& [id, f] : older.games)
        if (!this->games.count(id) && this->games.size() < MAX_FACTS) this->games.emplace(id, f);
}

std::string FactsBook::to_json() const
{
    nlohmann::json out = nlohmann::json::object();
    for (const auto& [id, f] : this->games) {
        char key[17];
        std::snprintf(key, sizeof key, "%016" PRIX64, id);
        nlohmann::json j = nlohmann::json::object();
        if (!f.name.empty()) j["name"] = f.name;
        if (!f.publisher.empty()) j["publisher"] = f.publisher;
        if (!f.developer.empty()) j["developer"] = f.developer;
        if (f.release_date) j["date"] = f.release_date;
        if (!f.categories.empty()) j["categories"] = f.categories;
        if (!f.languages.empty()) j["languages"] = f.languages;
        if (!f.rating.empty()) j["rating"] = f.rating;
        if (f.players) j["players"] = f.players;
        if (f.demo) j["demo"] = true;
        if (f.nsu_id) j["nsu"] = f.nsu_id;
        out[key] = std::move(j);
    }
    nlohmann::json root = { { "schema", SCHEMA }, { "games", std::move(out) } };
    return root.dump();
}

bool FactsBook::from_json(const std::string& text)
{
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object()) return false;
    const auto schema = root.find("schema");
    const auto list = root.find("games");
    if (schema == root.end() || !schema->is_number_integer() || schema->get<int>() != SCHEMA || list == root.end() ||
        !list->is_object())
        return false;
    this->games.clear();
    for (const auto& [key, j] : list->items()) {
        if (key.size() != 16 || key.find_first_not_of("0123456789ABCDEF") != std::string::npos || !j.is_object()) continue;
        Facts f;
        f.name = string_of(j, "name", 256);
        f.publisher = string_of(j, "publisher", 128);
        f.developer = string_of(j, "developer", 128);
        f.release_date = number_of<uint32_t>(j, "date", 99991231);
        f.categories = strings_of(j, "categories", 8, 64);
        f.languages = strings_of(j, "languages", 16, 8);
        f.rating = strings_of(j, "rating", 16, 64);
        f.players = number_of<int>(j, "players", 99);
        const auto demo = j.find("demo");
        f.demo = demo != j.end() && demo->is_boolean() && demo->get<bool>();
        f.nsu_id = number_of<uint64_t>(j, "nsu", UINT64_MAX);
        this->put(std::stoull(key, nullptr, 16), std::move(f));
    }
    return true;
}

std::vector<std::string> FactsBook::categories() const
{
    std::set<std::string> all;
    for (const auto& g : this->games) all.insert(g.second.categories.begin(), g.second.categories.end());
    return { all.begin(), all.end() };
}

std::vector<std::string> FactsBook::publishers() const
{
    std::set<std::string> all;
    for (const auto& g : this->games)
        if (!g.second.publisher.empty()) all.insert(g.second.publisher);
    return { all.begin(), all.end() };
}

size_t Filter::active() const
{
    return (size_t)!this->genre.empty() + (size_t)!this->publisher.empty() + (size_t)!this->language.empty() +
           (size_t)(this->players != 0) + (size_t)this->hide_demos + (size_t)(this->state != State::Any);
}

bool matches(const Filter& filter, const Facts& facts)
{
    auto has = [](const std::vector<std::string>& list, const std::string& v) {
        return std::find(list.begin(), list.end(), v) != list.end();
    };
    if (!filter.genre.empty() && !has(facts.categories, filter.genre)) return false;
    if (!filter.publisher.empty() && facts.publisher != filter.publisher) return false;
    if (!filter.language.empty() && !has(facts.languages, filter.language)) return false;
    if (filter.players == 1 && facts.players != 1) return false;
    if (filter.players >= 2 && facts.players < 2) return false;
    return !(filter.hide_demos && facts.demo);
}

uint32_t date_of(const std::string& ymd)
{
    if (ymd.size() != 10 || ymd[4] != '-' || ymd[7] != '-') return 0;
    uint32_t v = 0;
    for (size_t i : { 0, 1, 2, 3, 5, 6, 8, 9 }) {
        if (ymd[i] < '0' || ymd[i] > '9') return 0;
        v = v * 10 + (uint32_t)(ymd[i] - '0');
    }
    return v;
}

}   // namespace library
