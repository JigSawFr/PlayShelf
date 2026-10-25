// FooterHints — borealis' footer hints (brls:Hints), fitted to the footer.
// borealis gives the hints the width they ask for and the clock what is
// left: with five hints in French and the Switch's battery and Wi-Fi icons,
// the clock wrapped onto two lines and every hint ended in "…". Here the
// clock keeps its one line; the hints first come closer together (less room
// around each), and only when even that is not enough are their texts cut.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class FooterHints : public brls::Hints
{
  public:
    FooterHints();
    ~FooterHints() override;

    // Takes brls:Hints' place in borealis' footer. Called once, at start-up,
    // before the first screen opens.
    static void install();
    static brls::View* create();

    // The hint whose text is `hint_text` is on offer until `until` (µs,
    // brls::getCPUTimeUsec), out of `total_us`: a thin teal line under it
    // drains meanwhile, and the hint fades over its last second (ZR Undo).
    // An empty text, or `until` 0, ends it. Nothing with the motion off.
    static void set_timed(const std::string& hint_text, brls::Time until, brls::Time total_us);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    void fit();

    brls::VoidEvent::Subscription subscription;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);   // guards the fit after the first frame
};
