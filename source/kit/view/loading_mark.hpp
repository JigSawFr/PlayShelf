// LoadingMark — the PlayShelf icon (icon.jpg, from branding/icon.svg) drawn at
// the view's size for the loading screen, its corners rounded, over Aurore's
// aurora glow, which breathes from teal to violet while the app starts. Brand
// colours, not the theme's: the loading screen looks the same in both themes.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class LoadingMark : public brls::View
{
  public:
    LoadingMark();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    static brls::View* create();

  private:
    bool still = false;   // desktop smoke test: the glow at rest
};
