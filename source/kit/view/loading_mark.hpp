// LoadingMark — the PlayShelf icon (icon.jpg, from branding/icon.svg) drawn at
// the view's size for the loading screen, its corners rounded, over Aurore's
// aurora glow, which breathes from teal to violet while the app starts. Brand
// colours, not the theme's: the loading screen looks the same in both themes.
// A theme with a logo (`logo =`, Theme::logo()) shows it in the icon's place,
// over the same glow.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

class LoadingMark : public aurore::View
{
  public:
    LoadingMark();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              aurore::Style style, aurore::FrameContext* ctx) override;

    static aurore::View* create();

  private:
    bool still = false;   // desktop smoke test: the glow at rest
};
