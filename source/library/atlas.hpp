// atlas — the schema of Atlas' Switch files PlayShelf reads
// (https://github.com/JigSawFr/Atlas: platforms/switch/popular.json, games.json,
// similar.json, editions.json, upcoming.json, reach.json, also.json, changes.json and
// index.json). Atlas adds fields under the same schema and publishes a new one beside the old
// while PlayShelf reads it: a file of another schema is refused, and the
// copy PlayShelf had (fetched before, or the one it came with) kept.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <string_view>

namespace library
{

constexpr int ATLAS_SCHEMA = 1;

// The day a file of Atlas' says it was made ("2026-11-13"), read from its
// first bytes without parsing it: Atlas writes `generated` before `games`,
// within the first few hundred bytes. "" when it is not found there (the
// file is then parsed to know). Tells whether a file is worth parsing
// against one already read, megabytes of JSON saved at start-up.
inline std::string atlas_day_at_head(std::string_view text)
{
    constexpr std::string_view KEY = "\"generated\":\"";
    const std::string_view head = text.substr(0, 512);
    const size_t at = head.find(KEY);
    if (at == std::string_view::npos || head.find("\"games\"") < at) return "";
    const std::string_view day = head.substr(at + KEY.size(), 11);
    if (day.size() != 11 || day[10] != '"' || day[4] != '-' || day[7] != '-') return "";
    for (size_t i = 0; i < 10; i++)
        if (i != 4 && i != 7 && (day[i] < '0' || day[i] > '9')) return "";
    return std::string(day.substr(0, 10));
}

}   // namespace library
