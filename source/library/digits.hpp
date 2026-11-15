// digits — a number with its thousands grouped ("214 200", "214,200"), for the counts of a game's reviews and ratings.
//
// Plain C++ for the host tests (tests/library_digits).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace library
{

// `n` in groups of three digits from the right, `sep` between them. From five digits: a four digit count stays as it
// is (some languages group it and others do not).
std::string group_digits(uint64_t n, std::string_view sep);

}   // namespace library
