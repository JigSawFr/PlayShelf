// insight_notices — what action/insights made, said by the notification
// bell (action/notices): the updates and add-ons to get (Update all, the
// Library with those games), the games new on the servers (Home), the
// wishlist's games a server has (their page), on sale, out today or at a new
// lowest price (Atlas' prices and changes.json), the updates the servers lack
// yet, the SD card nearly full (Storage), the downloads installed or failed
// (Downloads). Here, with the screens, as each entry's action opens one.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "action/insights.hpp"

namespace insight_notices
{

// The bell's entries made again from `s`: set, replaced or cleared.
void publish(const insights::Snapshot& s);

}   // namespace insight_notices
