// CoverPlaceholder — what a game without a cover shows on its well (a tile,
// a cell, a download, its page): the theme's picture for it when the theme
// has one (the image slot SLOT, `image.playshelf/cover/placeholder` in a
// theme file: docs/themes.md), filling the well in its rounded corners; else
// PlayShelf's own, a gamepad in the quietest ink. It follows a theme or a
// variant chosen live, and a picture still loading leaves the gamepad.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class CoverPlaceholder : public brls::Box
{
  public:
    // The image slot's key, and the largest side its pictures are made at.
    static constexpr const char* SLOT = "playshelf/cover/placeholder";
    static constexpr int SLOT_SIDE = 512;

    // Declares the slot (no picture of PlayShelf's: the gamepad is drawn):
    // once, before the theme file of the preferences is read.
    static void declare();

    // `side` × `side`, a gamepad `glyph` units wide, the well's corner `radius`.
    CoverPlaceholder(float side, float glyph, float radius);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    float corner;
};
