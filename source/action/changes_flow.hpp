// changes_flow — what Atlas' last run changed (changes.json at the root of
// its data branch: library/changes), for the bell (activity/insight_notices):
// the wishlist's games out today, their sales started, their new lowest
// prices. Fetched through jsDelivr once a day while config popular_online
// allows it, only when Atlas' index.json says it changed
// (action/insights_index), kept in cache_dir(). Not shipped with PlayShelf:
// news goes stale, so a cached file older than STALE is not shown. Read off
// the UI thread; the book is the UI thread's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <memory>

#include "library/changes.hpp"

namespace changes_flow
{

// The cached file read, then the newest fetched when it is a day old. Once,
// at start-up, and again when popular_online changed.
void start();

// The last run's changes (UI thread); empty while none are known.
std::shared_ptr<const library::ChangeBook> book();

// Told when the book was read or replaced, on the UI thread.
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace changes_flow
