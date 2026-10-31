// updates_for_all — the update check made for all the Boréale homebrews
// installed, kept in config/boreale/updates.conf (boreale-link's bl_updates):
// the bells show another homebrew's update without the network.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "util/update.hpp"

namespace updates_for_all
{

// After PlayShelf's own check `own`: at most once a day, whoever made the last
// one, the newest release of each registered homebrew (its repository's
// compat document). Blocking (network): the update check's thread.
void check(const update::Result& own);

}   // namespace updates_for_all
