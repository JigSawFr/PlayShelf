// error_hint_logic — which hint an error code gets: one for each of
// PlayShelf's own results (core/nx_types.h), and one for the system results
// whose meaning is documented (switchbrew Error_codes, or seen and explained
// in PlayShelf's own reports). Any other code is shown alone, with the
// generic next step (ui::rc_text). No Aurore, so the host tests can run it
// (tests/error_hint_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>

namespace error_hint_logic
{

// The key under "playshelf/error/" of the hint for `rc`, or "" for none.
const char* hint_key(uint32_t rc);

}   // namespace error_hint_logic
