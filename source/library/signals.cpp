// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/signals.hpp"
#include "library/atlas.hpp"

#include <algorithm>
#include <cmath>

#include <aurore/extern/nlohmann/json.hpp>

namespace library
{

namespace
{
constexpr size_t MAX_TEXT = 120;

std::vector<Trait> traits_of(const nlohmann::json& j, const char* key)
{
    std::vector<Trait> out;
    const auto it = j.find(key);
    if (it == j.end() || !it->is_array()) return out;
    for (const auto& v : *it) {
        if (out.size() >= SignalBook::MAX_TRAITS) break;
        if (!v.is_array() || v.size() < 2 || !v[0].is_string() || !v[1].is_string()) continue;
        Trait t{ v[0].get<std::string>(), v[1].get<std::string>() };
        if (t.id.size() < 2 || t.id[0] != 'Q' || t.id.size() > 16) continue;
        if (t.label.size() > MAX_TEXT) t.label.resize(MAX_TEXT);
        // Control characters as spaces: a label never adds a line.
        for (char& c : t.label)
            if ((unsigned char)c < 0x20) c = ' ';
        out.push_back(std::move(t));
    }
    return out;
}

template <typename T>
T number_at(const nlohmann::json& j, const char* key)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer() || it->get<long long>() < 0) return 0;
    return (T)std::min<unsigned long long>(it->get<unsigned long long>(), (unsigned long long)~T(0));
}
}   // namespace

uint64_t base_id_of(const std::string& s)
{
    if (s.size() != 16) return 0;
    uint64_t id = 0;
    for (char c : s) {
        const int v = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (v < 0) return 0;
        id = id << 4 | (uint64_t)v;
    }
    return (id & 0xFFF) == 0 && !(id & 0x1000) ? id : 0;
}

bool is_day(const std::string& s)
{
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i = 0; i < s.size(); i++)
        if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
    return true;
}

bool SignalBook::from_json(const std::string& text)
{
    this->games.clear();
    this->day.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != ATLAS_SCHEMA || !root.contains("games") || !root["games"].is_object()) return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    for (const auto& [key, value] : root["games"].items()) {
        if (this->games.size() >= MAX_GAMES) break;
        const uint64_t id = base_id_of(key);
        if (!id || !value.is_object()) continue;
        GameSignals g;
        g.series = traits_of(value, "series");
        g.developers = traits_of(value, "developers");
        g.publishers = traits_of(value, "publishers");
        g.genres = traits_of(value, "genres");
        g.modes = traits_of(value, "modes");
        const auto steam = value.find("steam");
        if (steam != value.end() && steam->is_object()) {
            g.steam.app = number_at<uint32_t>(*steam, "app");
            g.steam.positive = number_at<uint64_t>(*steam, "positive");
            g.steam.negative = number_at<uint64_t>(*steam, "negative");
            g.steam.owners = number_at<uint64_t>(*steam, "owners");
            g.steam.ccu = number_at<uint32_t>(*steam, "ccu");
            if (!g.steam.app) g.steam = SteamAudience{};
        }
        // [rating 0..100, count], once PlayInsights has IGDB's keys.
        const auto igdb = value.find("igdb_rating");
        if (igdb != value.end() && igdb->is_array() && igdb->size() >= 2 && (*igdb)[0].is_number() &&
            (*igdb)[1].is_number_unsigned()) {
            const double rating = (*igdb)[0].get<double>();
            if (std::isfinite(rating) && rating > 0 && rating <= 100)
                g.igdb = { (uint32_t)std::lround(rating), (*igdb)[1].get<uint64_t>() };
        }
        this->games[id] = std::move(g);
    }
    this->day = generated->get<std::string>();
    return true;
}

const GameSignals* SignalBook::find(uint64_t base_id) const
{
    const auto it = this->games.find(base_id);
    return it == this->games.end() ? nullptr : &it->second;
}

bool SimilarBook::from_json(const std::string& text)
{
    this->games.clear();
    this->day.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != ATLAS_SCHEMA || !root.contains("games") || !root["games"].is_object()) return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    for (const auto& [key, value] : root["games"].items()) {
        if (this->games.size() >= MAX_GAMES) break;
        const uint64_t id = base_id_of(key);
        if (!id || !value.is_array()) continue;
        Row row;
        for (const auto& v : value) {
            if (row.size() >= MAX_SIMILAR) break;
            if (!v.is_array() || v.size() < 2 || !v[0].is_string() || !v[1].is_number_integer()) continue;
            const uint64_t other = base_id_of(v[0].get<std::string>());
            const int      score = std::clamp(v[1].get<int>(), 0, 100);
            if (other && other != id && score > 0) row.emplace_back(other, score);
        }
        std::stable_sort(row.begin(), row.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (!row.empty()) this->games[id] = std::move(row);
    }
    this->day = generated->get<std::string>();
    return true;
}

const SimilarBook::Row* SimilarBook::find(uint64_t base_id) const
{
    const auto it = this->games.find(base_id);
    return it == this->games.end() ? nullptr : &it->second;
}

int SimilarBook::between(uint64_t a, uint64_t b) const
{
    int best = 0;
    for (const auto& [from, to] : { std::pair{ a, b }, std::pair{ b, a } })
        if (const Row* row = this->find(from))
            for (const auto& [other, score] : *row)
                if (other == to) best = std::max(best, score);
    return best;
}

bool EditionBook::from_json(const std::string& text)
{
    this->to_game.clear();
    this->of_game.clear();
    this->day.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != ATLAS_SCHEMA || !root.contains("games") || !root["games"].is_object()) return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    for (const auto& [key, value] : root["games"].items()) {
        if (this->to_game.size() >= MAX_GAMES) break;
        const uint64_t edition = base_id_of(key);
        const uint64_t game = value.is_string() ? base_id_of(value.get<std::string>()) : 0;
        if (!edition || !game || edition == game) continue;
        this->to_game[edition] = game;
    }
    // One step only: an edition said to be one of another edition is one
    // of that edition's game.
    for (auto& [edition, game] : this->to_game) {
        const auto it = this->to_game.find(game);
        if (it != this->to_game.end() && it->second != edition) game = it->second;
    }
    for (const auto& [edition, game] : this->to_game)
        if (edition != game) this->of_game[game].push_back(edition);
    for (auto& [game, editions] : this->of_game) std::sort(editions.begin(), editions.end());
    this->day = generated->get<std::string>();
    return true;
}

uint64_t EditionBook::game_of(uint64_t id) const
{
    const auto it = this->to_game.find(id);
    return it == this->to_game.end() || it->second == id ? id : it->second;
}

std::vector<uint64_t> EditionBook::editions_of(uint64_t game) const
{
    const auto it = this->of_game.find(game);
    return it == this->of_game.end() ? std::vector<uint64_t>{} : it->second;
}

}   // namespace library
