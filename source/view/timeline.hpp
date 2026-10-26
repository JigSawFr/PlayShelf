// Timeline — a game's packages in the downloads as one line (Library ›
// Downloads): a segment each (the base game, its updates, its add-ons), as
// wide as its share of the sizes, a gap between them, each filled in the
// colour of its state: teal installed or being installed (up to where it
// stands), amber paused, coral failed, the well for what waits.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <vector>

#include "download/queue.hpp"

class Timeline : public brls::View
{
  public:
    Timeline();
    void set(std::vector<download::Segment> segments);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    std::vector<download::Segment> segments;
};
