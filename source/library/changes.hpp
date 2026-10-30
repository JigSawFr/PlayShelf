// changes — what Atlas' last run changed (https://github.com/JigSawFr/Atlas:
// changes.json, schema 1), for the Switch: the games out on the day of the
// run (`out_today`) and, by country, the sales that started and the new
// lowest prices (`sales`). The other platforms and the other kinds (new
// games, trends, sales ended) are skipped, as is any field not known here; a
// file of another schema is none. Plain C++ for the host tests
// (tests/library_changes).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

    // The ids read, in all.
    size_t size() const;

  private:
    struct Sales
    {
        std::unordered_set<uint64_t> started, lowest;
    };
    std::unordered_set<uint64_t>                 out;
    std::unordered_map<std::string, Sales>       sales;
    std::string                                  day;
};

}   // namespace library
