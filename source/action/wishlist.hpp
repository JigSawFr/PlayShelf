// wishlist — the games the user wants, by their base game's title id, kept
// in data_dir()/wishlist.txt: X on a game's page adds or takes one away.
// Home shows them, those a server has first; the bell says when one comes
// to a server. UI thread only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace wishlist
{

constexpr size_t MAX_GAMES = 500;

// Read once, at start-up.
void start();

bool has(uint64_t title_id);
// Added, or taken away when it was there; said in a toast with `name`.
void toggle(uint64_t title_id, const std::string& name);
// In the order they were added, the latest first.
std::vector<uint64_t> games();

// Told of every change, on the UI thread.
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace wishlist
