// nlib_flow — a game's page from Nlib API (titledb/nlib), when config
// nlib_api allows it: its details in the language PlayShelf speaks, asked
// off the UI thread and kept a week in cache_dir()/nlib/ (what data_cleanup
// empties); a game it does not know is not asked again until PlayShelf
// starts again. Its banner and screenshots come through library_icons'
// picture().
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "titledb/nlib.hpp"

namespace nlib_flow
{

// The details of the game `title_id` belongs to (its base game's):
// `done(details)` on the UI thread, nullptr when there are none (Nlib API
// off in the settings, out of reach, the game unknown to it).
void details(uint64_t title_id, std::function<void(std::shared_ptr<const titledb::Details>)> done);

}   // namespace nlib_flow
