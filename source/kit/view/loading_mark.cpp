// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/loading_mark.hpp"

#include <cmath>
#ifndef __SWITCH__
#include <cstdlib>
#include "core/sim_env.h"
#endif

namespace
{
// The brand palette (branding/*.svg).
const NVGcolor AMBER = nvgRGB(0xFF, 0xB5, 0x47);
const NVGcolor NAVY  = nvgRGB(0x0B, 0x14, 0x24);
const NVGcolor TRACK = nvgRGB(0x1F, 0x33, 0x55);
const NVGcolor TEAL  = nvgRGB(0x2E, 0xC4, 0xA6);
const NVGcolor LIGHT = nvgRGB(0xE8, 0xEE, 0xF7);

constexpr float PI = 3.14159265f;
// One turn of the arc, in seconds.
constexpr float TURN_S = 1.6f;
// The icon's arc: 292 of the ring's 390 units (stroke-dasharray), from the top.
constexpr float ARC = 292.0f / 390.0f * 2.0f * PI;
}   // namespace

LoadingMark::LoadingMark()
{
    this->setFocusable(false);
#ifndef __SWITCH__
    // The desktop smoke test compares screenshots: no movement there.
    this->still = sim_env_flag("PLAYSHELF_SIM_STILL_FOCUS");
#endif
}

void LoadingMark::draw(NVGcontext* vg, float x, float y, float width, float height,
                       brls::Style style, brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;
    const float size = std::fmin(width, height);
    const float cx = x + width / 2.0f, cy = y + height / 2.0f;
    const float u = size / 256.0f;   // one unit of branding/icon.svg

    // A soft teal glow behind the tile, past the view's bounds.
    NVGpaint glow = nvgRadialGradient(vg, cx, cy, size * 0.35f, size * 1.25f, nvgRGBA(0x2E, 0xC4, 0xA6, 46),
                                      nvgRGBA(0x2E, 0xC4, 0xA6, 0));
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, size * 1.25f);
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    // The tile, rounded as the HOME menu rounds the icon.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, cx - size / 2.0f, cy - size / 2.0f, size, size, size * 0.22f);
    nvgFillColor(vg, AMBER);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 78.0f * u);
    nvgFillColor(vg, NAVY);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 62.0f * u);
    nvgStrokeColor(vg, TRACK);
    nvgStrokeWidth(vg, 16.0f * u);
    nvgStroke(vg);

    // The arc, turning clockwise from where the icon has it.
    float start = -PI / 2.0f;
    if (!this->still) {
        const double t = (double)brls::getCPUTimeUsec() / 1e6;
        start += (float)std::fmod(t / TURN_S, 1.0) * 2.0f * PI;
    }
    nvgBeginPath(vg);
    nvgArc(vg, cx, cy, 62.0f * u, start, start + ARC, NVG_CW);
    nvgStrokeColor(vg, TEAL);
    nvgStrokeWidth(vg, 16.0f * u);
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);
    nvgLineCap(vg, NVG_BUTT);

    // The play button, its corners rounded by its stroke.
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx - 16.0f * u, cy - 28.0f * u);
    nvgLineTo(vg, cx + 32.0f * u, cy);
    nvgLineTo(vg, cx - 16.0f * u, cy + 28.0f * u);
    nvgClosePath(vg);
    nvgFillColor(vg, LIGHT);
    nvgFill(vg);
    nvgStrokeColor(vg, LIGHT);
    nvgStrokeWidth(vg, 8.0f * u);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStroke(vg);
    nvgLineJoin(vg, NVG_MITER);
}

brls::View* LoadingMark::create()
{
    return new LoadingMark();
}
