// What a sheet over the screen (ui::pick() and the others of action_sheet.cpp,
// the icon sheet of a forwarder) shares: closing it, and the room around its
// panel, where a touch closes it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>

namespace ui
{

// Pops the sheet (a fade), then runs `then` (if any) on the next frame.
void close_sheet(std::function<void()> then);

// Room around the panel, `width` wide or taking what is left (`grow`): a
// touch there closes the sheet.
brls::Box* sheet_outside(float width, bool grow);

}   // namespace ui
