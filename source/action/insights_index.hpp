// insights_index — PlayInsights' index.json, so popular_flow and prices_flow
// download only the files that changed: fetched at most once a day (kept in
// cache_dir()), each file's cached copy compared with it by size and hash
// (library/insights_index). A copy found unchanged is marked checked, and
// counts as fetched today. Without an index, or for a file it does not list,
// the files are fetched once a day as before. Safe from any worker.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace insights_index
{

// On a worker: whether the cached copy at `path` of PlayInsights' `name`
// (switch/popular.json, switch/prices/FR.json) is still the one published. When it is,
// it is marked checked.
bool unchanged(const std::string& name, const std::string& path);
// The age in seconds of the cached copy at `path`, from when it was last
// fetched or found unchanged; -1 when there is none.
long age(const std::string& path);
// The index known forgotten, so the next unchanged() asks for it again (a
// refresh from Preferences).
void forget();

}   // namespace insights_index
