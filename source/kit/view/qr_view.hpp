// QrView — a QR code of a text (a URL), dark modules on white with the
// four-module quiet zone, whatever the theme: phones read it best that way.
// Square: the code fills the smaller side, in whole pixels per module.
// Drawn as one texture, scaled without smoothing: a path per module was a
// draw call each, and a long link (the bug-report form with a gist link)
// overflowed the frame's GPU command memory, which aborts the app.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

class QrView : public brls::View
{
  public:
    QrView(const std::string& text, float side);
    ~QrView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    int               size = 0;    // modules per side (0: the text could not be encoded)
    std::vector<bool> modules;     // row by row, true = dark
    int               image = 0;   // the texture, made at the first draw (0: none yet)
};
