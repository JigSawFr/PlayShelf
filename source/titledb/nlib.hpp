// nlib — Nlib API (https://api.nlib.cc/nx/), where an icon no title
// database has is asked for: the address of a base game's icon there. It is
// built here, never taken from the API's answers (they give http://), and
// asked for one game at a time, the one on screen.
//
// Plain C++ for the host tests (tests/titledb).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace titledb
{

// The size Nlib API is asked for: a cover's, and the database's own.
constexpr int NLIB_ICON_SIZE = 256;

// The icon of the base game `base_id`, `size` pixels square (30 to 4096:
// the API serves the nearest ten); "" for an id that is not a base game's
// (Nlib API knows no update and no add-on) or a size out of range.
std::string nlib_icon_url(uint64_t base_id, int size = NLIB_ICON_SIZE);

}   // namespace titledb
