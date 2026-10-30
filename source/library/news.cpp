// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/news.hpp"

#include <algorithm>
#include <format>
#include <set>

#include <aurore/extern/nlohmann/json.hpp>

#include "library/seen.hpp"

namespace library
{

namespace
{
const char* key_of(NewsKind kind) { return kind == NewsKind::New ? "new" : "trend"; }

// The series of these games, by Wikidata id: its label (the first one known).
std::map<std::string, std::string> series_of(const SignalBook& signals, const std::vector<uint64_t>& games)
{
    std::map<std::string, std::string> out;
    for (uint64_t id : games)
        if (const GameSignals* g = signals.find(id))
            for (const Trait& t : g->series)
                if (!t.label.empty()) out.emplace(t.id, t.label);
    return out;
}

// The first of a game's series found in `mine`, "" when none.
std::string shared_series(const SignalBook& signals, uint64_t id, const std::map<std::string, std::string>& mine)
{
    if (const GameSignals* g = signals.find(id))
        for (const Trait& t : g->series)
            if (const auto it = mine.find(t.id); it != mine.end()) return it->second;
    return "";
}
}   // namespace

bool TellBook::may_tell(NewsKind kind, uint64_t id, uint32_t day) const
{
    const auto& told = this->of(kind);
    const auto  it = told.find(id);
    return it == told.end() || it->second == day;
}

bool TellBook::note(NewsKind kind, uint64_t id, uint32_t day)
{
    auto& told = this->of(kind);
    if (told.contains(id)) return false;
    if (told.size() >= MAX_GAMES) {
        auto oldest = std::min_element(told.begin(), told.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        told.erase(oldest);
    }
    told[id] = day;
    return true;
}

bool TellBook::forget_older(uint32_t today)
{
    bool changed = false;
    for (auto* told : { &this->told_new, &this->told_trend })
        changed |= std::erase_if(*told, [today](const auto& e) { return days_between(e.second, today) > (long)KEEP_DAYS; }) > 0;
    return changed;
}

std::string TellBook::to_json() const
{
    nlohmann::json root = this->unknown.empty() ? nlohmann::json::object() : nlohmann::json::parse(this->unknown, nullptr, false);
    if (!root.is_object()) root = nlohmann::json::object();
    root["schema"] = SCHEMA;
    for (NewsKind kind : { NewsKind::New, NewsKind::Trend }) {
        nlohmann::json list = nlohmann::json::object();
        for (const auto& [id, day] : this->of(kind)) list[std::format("{:016X}", id)] = day;
        root[key_of(kind)] = std::move(list);
    }
    return root.dump();
}

bool TellBook::from_json(const std::string& text)
{
    this->told_new.clear();
    this->told_trend.clear();
    this->unknown.clear();
    nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != SCHEMA) return false;
    for (NewsKind kind : { NewsKind::New, NewsKind::Trend }) {
        const auto list = root.find(key_of(kind));
        if (list == root.end() || !list->is_object()) continue;
        auto& told = this->of(kind);
        for (const auto& [key, day] : list->items()) {
            if (told.size() >= MAX_GAMES) break;
            const uint64_t id = base_id_of(key);
            if (!id || !day.is_number_unsigned()) continue;
            const uint64_t d = day.get<uint64_t>();
            if (d >= 19000101 && d <= 99991231) told[id] = (uint32_t)d;
        }
    }
    root.erase("schema");
    root.erase("new");
    root.erase("trend");
    if (!root.empty()) this->unknown = root.dump();
    return true;
}

bool TellBook::newer(const std::string& text)
{
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object()) return false;
    const auto schema = root.find("schema");
    return schema != root.end() && schema->is_number_unsigned() && schema->get<uint64_t>() > (uint64_t)SCHEMA;
}

std::vector<News> news_of(const ChangeBook& changes, const SignalBook& signals, const SimilarBook& similar,
                          const EditionBook& editions, const std::vector<uint64_t>& owned,
                          const std::vector<uint64_t>& wished, const TellBook& told, size_t max)
{
    std::vector<News> out;
    const uint32_t    day = changes.day_number();
    if (!day) return out;
    std::set<uint64_t> have, wish;
    for (uint64_t id : owned) have.insert(editions.game_of(id));
    for (uint64_t id : wished) wish.insert(editions.game_of(id));
    std::vector<uint64_t> mine(owned);
    mine.insert(mine.end(), wished.begin(), wished.end());
    const auto mine_series = series_of(signals, mine), owned_series = series_of(signals, owned);
    const auto left_out = [&](NewsKind kind, uint64_t id) {
        const uint64_t game = editions.game_of(id);
        return have.contains(game) || !told.may_tell(kind, id, day);
    };

    // New: of a series first, then the most alike.
    std::vector<std::pair<int, News>> fresh;
    for (uint64_t id : changes.new_games()) {
        if (left_out(NewsKind::New, id) || wish.contains(editions.game_of(id))) continue;
        News n;
        n.kind = NewsKind::New;
        n.id = id;
        n.series = shared_series(signals, id, mine_series);
        if (!n.series.empty()) {
            fresh.push_back({ 101, std::move(n) });
            continue;
        }
        int best = 0;
        for (uint64_t m : mine)
            if (const int s = similar.between(id, m); s > best) {
                best = s;
                n.like = m;
            }
        if (best < NEWS_ALIKE) continue;
        n.why = News::Why::Alike;
        fresh.push_back({ best, std::move(n) });
    }
    std::stable_sort(fresh.begin(), fresh.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (size_t i = 0; i < fresh.size() && i < max; i++) out.push_back(std::move(fresh[i].second));

    // Trends: risen, the wishlist's first, then the biggest move.
    std::vector<std::pair<int, News>> risen;
    for (const auto& [id, t] : changes.trends()) {
        if (t.now <= t.before || left_out(NewsKind::Trend, id)) continue;
        News n;
        n.kind = NewsKind::Trend;
        n.id = id;
        n.before = t.before;
        n.now = t.now;
        if (wish.contains(editions.game_of(id)))
            n.why = News::Why::Wished;
        else if ((n.series = shared_series(signals, id, owned_series)).empty())
            continue;
        risen.push_back({ (n.why == News::Why::Wished ? 10000 : 0) + t.now - t.before, std::move(n) });
    }
    std::stable_sort(risen.begin(), risen.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (size_t i = 0; i < risen.size() && i < max; i++) out.push_back(std::move(risen[i].second));
    return out;
}

}   // namespace library
