// news — of what Atlas' last run changed (library/changes), what the bell
// tells the console's player beyond the wishlist's sales and releases:
// - a game new on the Switch (`new`) of a series one of the console's or
//   the wishlist's games is of, or much like one of them (similar.json,
//   ALIKE on);
// - a game read much more lately (`trends`, risen by 100 points or more),
//   of the wishlist, or of a series of the console's games.
// Never a game the console has (or an edition of one), nor a new game
// already on the wishlist; MAX of each kind, the closest first. Each game
// told once (TellBook, news.json in the data folder): its entry lasts as
// long as the run that told it is Atlas' last one, and the game is not told
// again for the same kind of news, whatever the day.
//
// Plain C++ for the host tests (tests/library_news).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "library/changes.hpp"
#include "library/signals.hpp"

namespace library
{

enum class NewsKind
{
    New,     // new on the Switch
    Trend,   // read much more lately
};

struct News
{
    enum class Why
    {
        Series,   // of `series`, which the console's (or the wishlist's) games are of
        Alike,    // much like `like`, a game of the console or of the wishlist
        Wished,   // on the wishlist
    };
    NewsKind    kind = NewsKind::New;
    Why         why = Why::Series;
    uint64_t    id = 0;
    std::string series;
    uint64_t    like = 0;
    int         before = 0, now = 0;   // a trend's
};

// The games told once, and by which run (YYYYMMDD), by kind of news.
class TellBook
{
  public:
    static constexpr int      SCHEMA = 1;
    static constexpr uint32_t KEEP_DAYS = 365;   // then a game may be told again
    static constexpr size_t   MAX_GAMES = 2000;  // of a kind: the oldest go first

    // Whether `id` may be told by the run of `day`: never told, or told by
    // that same run.
    bool may_tell(NewsKind kind, uint64_t id, uint32_t day) const;
    // Told by the run of `day`. True when the book changed (to be written).
    bool note(NewsKind kind, uint64_t id, uint32_t day);
    // Forgets what was told KEEP_DAYS before `today`. True when it changed.
    bool forget_older(uint32_t today);

    // news.json: {"schema": 1, "new": {"<id>": YYYYMMDD}, "trend": {…}};
    // the keys this build does not know, read, are written back.
    std::string to_json() const;
    // False when it is not one (another schema): the book is left empty.
    bool from_json(const std::string& text);
    // Whether news.json's text is of a newer schema: not read, never written over.
    static bool newer(const std::string& text);

    size_t size() const { return this->told_new.size() + this->told_trend.size(); }

  private:
    std::map<uint64_t, uint32_t>       told_new, told_trend;
    std::string                        unknown;   // the other keys read, as a JSON object
    std::map<uint64_t, uint32_t>&       of(NewsKind kind) { return kind == NewsKind::New ? this->told_new : this->told_trend; }
    const std::map<uint64_t, uint32_t>& of(NewsKind kind) const
    {
        return kind == NewsKind::New ? this->told_new : this->told_trend;
    }
};

constexpr int    NEWS_ALIKE = 60;   // similar.json's likeness from which a new game is told
constexpr size_t NEWS_MAX = 3;      // of each kind, a run

// What the run of `changes` has worth telling, the games `told` already
// left out (by an earlier run); `owned` the console's games, `wished` the
// wishlist's.
std::vector<News> news_of(const ChangeBook& changes, const SignalBook& signals, const SimilarBook& similar,
                          const EditionBook& editions, const std::vector<uint64_t>& owned,
                          const std::vector<uint64_t>& wished, const TellBook& told, size_t max = NEWS_MAX);

}   // namespace library
