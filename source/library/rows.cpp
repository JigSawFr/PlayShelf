// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/rows.hpp"

#include <algorithm>

namespace library
{

Together together_of(const GameSignals& game)
{
    Together best = Together::None;
    for (const Trait& mode : game.modes) {
        if (mode.id == "Q61005756" || mode.id == "Q1630770") return Together::SameScreen;   // shared/split screen, hotseat
        if (mode.id == "Q1758804" || mode.id == "Q107494132") best = Together::Coop;        // co-op, local co-op
    }
    return best;
}

double tonight_quality(uint64_t play_s, uint32_t launches, const Renown& renown, int players_rating)
{
    if (play_s >= TONIGHT_PLAY_S || launches >= TONIGHT_LAUNCHES || renown.score < TONIGHT_RENOWN) return 0;
    const double quality = quality_of(renown, players_rating);
    return quality >= TONIGHT_QUALITY ? quality : 0;
}

bool read_in_language(const ReachBook& reach, uint64_t base_id, const std::string& language)
{
    return !language.empty() && language != "en" && reach.share(base_id, language) >= LOCAL_SHARE;
}

Series series_of(const std::vector<Owned>& owned, const SignalBook& signals)
{
    Series mine;
    for (const Owned& o : owned)
        if (const GameSignals* g = signals.find(o.id))
            for (const Trait& t : g->series) {
                auto& m = mine[t.id];
                if (o.weight > m.first) m = { o.weight, t.label };
            }
    return mine;
}

std::vector<SeriesGame> series_gaps(const SignalBook& signals, const Series& mine, const std::function<bool(uint64_t)>& excluded,
                                    const std::function<int(uint64_t)>& renown, size_t max)
{
    std::vector<std::pair<double, SeriesGame>> found;
    for (const auto& [id, g] : signals.all()) {
        if (g.name.empty() || excluded(id)) continue;
        for (const Trait& t : g.series) {
            const auto it = mine.find(t.id);
            if (it == mine.end() || it->second.second.empty()) continue;
            found.push_back({ it->second.first + renown(id) / 1e3, { id, g.name, it->second.second } });
            break;
        }
    }
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second.id < b.second.id;
    });
    std::vector<SeriesGame> out;
    for (auto& f : found) {
        if (out.size() >= max) break;
        out.push_back(std::move(f.second));
    }
    return out;
}

}   // namespace library
