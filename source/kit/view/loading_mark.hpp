// LoadingMark — the PlayShelf icon drawn at the view's size for the loading
// screen: the amber tile, the navy disc, the play-time ring and the play
// button (branding/icon.svg), with a soft teal glow behind it. The ring's teal
// arc turns while the app starts. Brand colours, not the theme's: the loading
// screen looks the same in both themes, like the store banner.
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
    bool still = false;   // desktop smoke test: the arc where the icon has it
};
