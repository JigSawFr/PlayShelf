// LoadingBar — a progress bar that eases: the loading screen's (a rounded navy
// track, filled in the brand teal up to the start-up step reached), and a
// screen's while its list loads, in the theme's colours. The fill eases to each
// new value instead of jumping (the next step's frames show it moving), or,
// for the intro, fills at a steady pace over a given time. Brand colours, not
// the theme's (see LoadingMark).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class LoadingBar : public brls::View
{
  public:
    LoadingBar();

    // 0..1: how much of the start-up is done.
    void setProgress(float value);
    // Fills to `value` at a steady pace, over `duration_us`.
    void fillOver(float value, brls::Time duration_us);
    // Empty again at once (a bar shown anew, not easing back down).
    void reset();
    // In the brand's colours whatever the theme (the loading screen's); else
    // the theme's well and teal (a screen's, while its list loads).
    void setBrand(bool on) { this->brand = on; }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    static brls::View* create();

  private:
    void  moveTo(float value, float duration_us, bool at_steady_pace);
    float drawn(brls::Time now) const;   // the fill at `now`

    float from = 0.0f, to = 0.0f;   // the fill goes from `from` to `to` ...
    brls::Time since = 0;           // ... starting then (µs) ...
    float duration = 0.0f;          // ... in that long (µs) ...
    bool  steady = false;           // ... at a steady pace, or easing out
    bool  brand = false;
};
