// game_page — a catalogue's game opened on its page (activity/game_activity),
// from the Catalogue and the Home tab: its lines, its icon (the server's,
// else the title database's, else Nlib API's), and more of the catalogue's
// games of its publisher and of its genre (as far as their facts are known
// yet), each opening its own page.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <memory>

#include "action/library_flow.hpp"

namespace game_page
{

// The game `index` of data->titles.
void open(const std::shared_ptr<const library_flow::Loaded>& data, size_t index);

}   // namespace game_page
