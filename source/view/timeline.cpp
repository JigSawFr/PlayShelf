// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/timeline.hpp"

#include <algorithm>

namespace
{
constexpr float GAP = 4;
}   // namespace

Timeline::Timeline()
{
    this->setHeight(12);
    this->setFocusable(false);
}

void Timeline::set(std::vector<download::Segment> s)
{
    this->segments = std::move(s);
}

void Timeline::draw(NVGcontext* vg, float x, float y, float width, float height, aurore::Style, aurore::FrameContext*)
{
    if (this->segments.empty()) return;
    aurore::Theme theme = aurore::Application::getTheme();
    const float room = width - GAP * (float)(this->segments.size() - 1);
    const float radius = height / 2.0f;
    float       at = x;
    for (const download::Segment& s : this->segments) {
        // Never thinner than a dot: a small add-on beside a large game.
        const float w = std::max(height, room * (float)s.weight);
        const float right = std::min(x + width, at + w);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, at, y, right - at, height, radius);
        nvgFillColor(vg, this->a(theme["cadran/well"]));
        nvgFill(vg);
        const char* key = s.state == download::State::Paused   ? "cadran/amber_fill"
                        : s.state == download::State::Failed   ? "cadran/coral_fill"
                                                               : "cadran/teal_fill";
        const float filled = s.state == download::State::Failed ? 1.0f : (float)s.filled;
        if (filled > 0) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, at, y, std::max(height, (right - at) * filled), height, radius);
            nvgFillColor(vg, this->a(theme[key]));
            nvgFill(vg);
        }
        at = right + GAP;
    }
}
