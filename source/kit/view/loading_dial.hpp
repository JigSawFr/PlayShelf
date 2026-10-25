// LoadingDial — Cadran's dial on the right of the loading screen, behind the
// icon and the bar, as the PIN gate draws it (core/gate_screen.c's dial()):
// its teal glow, two rings, the minutes, the day's arc. Brand colours (see
// LoadingMark); its lines in navy on the light theme's white.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class LoadingDial : public brls::View
{
  public:
    LoadingDial();

    // The light theme: its lines in navy instead of white.
    void setLight(bool on) { this->light = on; }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    static brls::View* create();

  private:
    bool light = false;
};
