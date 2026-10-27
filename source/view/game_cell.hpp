// GameCell — a game in a list (the Library's list view): its icon on the
// well (a gamepad until one arrives, or for a game without one), its name
// and a value at the end of the line. From PlayGuard's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <cstdint>

class GameCell : public brls::DetailCell
{
  public:
    GameCell();

    // Its icon (width × height RGBA pixels) in place of the gamepad.
    void set_icon(const uint8_t* rgba, int width, int height);

  private:
    brls::Box*   slot = nullptr;   // the well, the gamepad, the icon
    brls::View*  placeholder = nullptr;
    brls::Image* icon = nullptr;
};
