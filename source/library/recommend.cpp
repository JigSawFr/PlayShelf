// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/recommend.hpp"

#include <algorithm>
#include <cmath>

#include "library/seen.hpp"

namespace library
{

namespace
{
// How much each signal counts in a suggestion's score.
constexpr double LIKE = 1.6, SERIES = 1.0, DEVELOPER = 0.5, GENRE = 0.6, QUALITY = 0.5, FRESH = 0.15;
constexpr double UNWANTED = 1.6;   // as much as a likeness: one cancels the other
constexpr uint64_t STEAM_TRUSTED = 50;   // reviews below which Steam's say is left out

// A trait of `kinds` shared with the console's games: the weightiest such
// game's weight (relative), and the trait's label.
struct Shared
{
    double      weight = 0;
    std::string label;
};

Shared shared(const std::vector<Trait>& mine, const std::vector<Owned>& owned, const SignalBook& signals, double top,
              std::vector<Trait> GameSignals::*kind)
{
    Shared best;
    if (mine.empty() || top <= 0) return best;
    for (const Owned& o : owned) {
        const GameSignals* g = signals.find(o.id);
        if (!g || o.weight / top <= best.weight) continue;
        for (const Trait& t : g->*kind)
            if (std::any_of(mine.begin(), mine.end(), [&](const Trait& m) { return m.id == t.id; })) {
                best = { o.weight / top, t.label };
                break;
            }
    }
    return best;
}
}   // namespace

double owned_weight(uint64_t play_s, uint32_t launches, uint64_t last_played, uint64_t now)
{
    if (!play_s && !launches) return 0.3;
    double w = 1.0 + std::log2(1.0 + (double)play_s / 3600.0);
    if (last_played && now >= last_played && now - last_played < 30ULL * 86400) w *= 1.5;
    return w;
}

std::vector<Suggestion> recommend(const std::vector<Owned>& owned, const std::vector<Candidate>& candidates,
                                  const std::map<std::string, double>& genres, const SimilarBook& similar,
                                  const SignalBook& signals, const std::vector<uint64_t>& unwanted, uint32_t today,
                                  size_t max)
{
    double top = 0;
    for (const Owned& o : owned) top = std::max(top, o.weight);
    double genre_top = 0;
    for (const auto& g : genres) genre_top = std::max(genre_top, g.second);

    std::vector<Suggestion> out;
    for (const Candidate& c : candidates) {
        // One the player does not want: never; like one: less.
        if (std::find(unwanted.begin(), unwanted.end(), c.id) != unwanted.end()) continue;
        double unlike = 0;
        for (uint64_t u : unwanted) unlike = std::max(unlike, similar.between(u, c.id) / 100.0);
        // Like the console's games: each one's likeness, by its weight.
        double   like = 0, like_best = 0;
        uint64_t because = 0;
        if (top > 0)
            for (const Owned& o : owned) {
                const int s = similar.between(o.id, c.id);
                if (!s) continue;
                const double part = (o.weight / top) * s / 100.0;
                like += part;
                if (part > like_best) like_best = part, because = o.id;
            }
        like = std::min(like, 1.0);
        // Its series and developers among the console's games'.
        const GameSignals* mine = signals.find(c.id);
        const Shared       series = mine ? shared(mine->series, owned, signals, top, &GameSignals::series) : Shared{};
        const Shared       developer = mine ? shared(mine->developers, owned, signals, top, &GameSignals::developers) : Shared{};
        // Its genres among the console's games', the weightiest.
        double      genre = 0;
        std::string genre_label;
        if (genre_top > 0)
            for (const std::string& g : c.genres) {
                const auto it = genres.find(g);
                if (it != genres.end() && it->second / genre_top > genre) genre = it->second / genre_top, genre_label = g;
            }
        // How good it is said to be: its renown, its critics, Steam's players.
        double quality = 0.5 * c.renown.score / 100.0 + 0.3 * c.renown.critics / 100.0;
        if (mine && mine->steam.positive + mine->steam.negative >= STEAM_TRUSTED)
            quality += 0.2 * (double)mine->steam.positive / (double)(mine->steam.positive + mine->steam.negative);
        // Out in the last year: a little more.
        double fresh = 0;
        if (c.release_date && today) {
            const long days = days_between(c.release_date, today);
            if (days >= 0 && days < 365) fresh = 1.0 - days / 365.0;
        }

        const double parts[] = { LIKE * like, SERIES * series.weight, DEVELOPER * developer.weight, GENRE * genre,
                                 QUALITY * quality };
        const double score = parts[0] + parts[1] + parts[2] + parts[3] + parts[4] + FRESH * fresh - UNWANTED * unlike;
        // Nothing of the player's taste and little renown, or more like what
        // they do not want than anything: not worth saying.
        if (parts[0] + parts[1] + parts[2] + parts[3] <= 0 && quality < 0.3) continue;
        if (score <= 0) continue;
        Suggestion s;
        s.id = c.id;
        s.score = score;
        const size_t why = (size_t)(std::max_element(std::begin(parts), std::end(parts)) - std::begin(parts));
        switch (why) {
            case 0: s.reason = { Reason::Kind::Like, because, "" }; break;
            case 1: s.reason = { Reason::Kind::Series, 0, series.label }; break;
            case 2: s.reason = { Reason::Kind::Developer, 0, developer.label }; break;
            case 3: s.reason = { Reason::Kind::Genre, 0, genre_label }; break;
            default: s.reason = { Reason::Kind::Popular, 0, "" }; break;
        }
        // A series or a studio without a name says nothing: its likeness instead.
        if ((s.reason.kind == Reason::Kind::Series || s.reason.kind == Reason::Kind::Developer) && s.reason.label.empty())
            s.reason = because ? Reason{ Reason::Kind::Like, because, "" } : Reason{ Reason::Kind::Popular, 0, "" };
        out.push_back(std::move(s));
    }
    std::stable_sort(out.begin(), out.end(), [](const Suggestion& a, const Suggestion& b) { return a.score > b.score; });
    if (out.size() > max) out.resize(max);
    return out;
}

}   // namespace library
