// newer_files — PlayShelf's files that a newer PlayShelf wrote. Each reader
// leaves such a file as it is (never read, never written over) and says so in
// one line of the log; this tells the user once, in the bell, so the missing
// servers or queue are not a mystery. UI thread only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace newer_files
{

// The names (servers.json…) of the files in the data folder whose schema is
// newer than this build reads, in a fixed order; none when all are readable
// or absent.
std::vector<std::string> list();

// Sets the bell's entry for information with those names, or clears it when
// there are none. Once at start-up, before the bell is published.
void notify();

}   // namespace newer_files
