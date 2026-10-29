// MadeInFrance — a small French flag and "Made in France", for the About tab
// and the first steps. The flag is drawn (blue, white, red bands, with a thin
// outline so the white band shows on the light theme): the console's shared
// font has no emoji, so "🇫🇷" would not show.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

class FrenchFlag : public aurore::View
{
  public:
    FrenchFlag();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              aurore::Style style, aurore::FrameContext* ctx) override;
};

class MadeInFrance : public aurore::Box
{
  public:
    MadeInFrance();
    static aurore::View* create();
};
