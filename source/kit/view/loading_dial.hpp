// LoadingDial — Cadran's dial on the right of the loading screen, behind the
// icon and the bar, as the PIN gate draws it (core/gate_screen.c's dial()):
// its teal glow, two rings, the minutes, the day's arc. Its colours are
// theme keys of its own (KEYS below, drawn as they are when drawing), so a
// theme file can restyle it: its lines white over the navy, navy on the
// light theme's white.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class LoadingDial : public brls::View
{
  public:
    LoadingDial();

    // Its theme keys, in both variants' base colours: added once, before
    // borealis starts (a theme file read then knows them).
    static void addColors();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    static brls::View* create();
};
