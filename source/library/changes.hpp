// changes — what Atlas' last run changed (https://github.com/JigSawFr/Atlas:
// changes.json, schema 1), for the Switch: the games out on the day of the
// run (`out_today`), the games new on Atlas since its last run (`new`), the
// trends that moved by 100 points or more (`trends`: `[id, before, now]`)
// and, by country, the sales that started and the new lowest prices
// (`sales`). The other platforms and the sales ended are skipped, as is any
// field not known here; a file of another schema is none. Plain C++ for the host tests
// (tests/library_changes).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace library
{

class ChangeBook
{
  public:
    static constexpr size_t MAX_IDS = 1000;   // of each list read: Atlas writes 200 at most

    // From changes.json; false when it is not one (another schema, no day):
    // the book is left empty.
    bool from_json(const std::string& text);

    // The day of the run, YYYY-MM-DD ("" when empty), and as YYYYMMDD.
    const std::string& generated() const { return this->day; }
    uint32_t           day_number() const;

    // By a base game's title id; `country` one of PRICE_COUNTRIES.
    bool out_today(uint64_t id) const { return this->out.contains(id); }
    bool sale_started(const std::string& country, uint64_t id) const;
    bool new_lowest(const std::string& country, uint64_t id) const;

    // A trend's move (popular.json's `trend`: how much more the game was
    // read lately, in percent), before the run and after it.
    struct Trend
    {
        int before = 0, now = 0;
    };
    // The games new since Atlas' last run, and the trends that moved, in the
    // file's order, each game once.
    const std::vector<uint64_t>&                    new_games() const { return this->fresh; }
    const std::vector<std::pair<uint64_t, Trend>>&  trends() const { return this->moved; }

    // The ids read, in all.
    size_t size() const;

  private:
    struct Sales
    {
        std::unordered_set<uint64_t> started, lowest;
    };
    std::unordered_set<uint64_t>                 out;
    std::vector<uint64_t>                        fresh;
    std::vector<std::pair<uint64_t, Trend>>      moved;
    std::unordered_map<std::string, Sales>       sales;
    std::string                                  day;
};

}   // namespace library
