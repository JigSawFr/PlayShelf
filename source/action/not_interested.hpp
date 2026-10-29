// not_interested — the games the player does not want suggested, by their
// base game's title id, kept in data_dir()/not_interested.txt: X on one of
// Home's suggestions puts it here; Preferences shows them all again. The
// recommendations (library/recommend) leave them out and count against the
// games much like them. UI thread only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace not_interested
{

constexpr size_t MAX_GAMES = 500;

// Read once, at start-up.
void start();

// Put away, said in a toast with `name`.
void add(uint64_t title_id, const std::string& name);
// All of them suggested again.
void clear();
// The latest first.
std::vector<uint64_t> games();

// Told of every change, on the UI thread.
int  listen(std::function<void()> changed);
void unlisten(int id);

}   // namespace not_interested
