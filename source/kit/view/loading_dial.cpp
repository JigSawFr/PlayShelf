// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/loading_dial.hpp"

#include <cmath>
#include <utility>

namespace
{
constexpr float PI = 3.14159265f;
// Where the gate has it on its 1280 x 720 screen (core/gate_screen.c).
constexpr float CX = 1196.0f, CY = 340.0f;
}   // namespace

LoadingDial::LoadingDial()
{
    this->setFocusable(false);
}

void LoadingDial::draw(NVGcontext* vg, float x, float y, float width, float height,
                       brls::Style style, brls::FrameContext* ctx)
{
    (void)width;
    (void)height;
    (void)style;
    (void)ctx;
    const float cx = x + CX, cy = y + CY;
    // The lines: white over the navy, navy (twice as strong) over the light theme's white.
    const bool on_white = this->light;
    const auto line = [on_white](float strength) {
        const unsigned char a = (unsigned char)std::lround((on_white ? strength * 2.0f : strength) * 255.0f);
        return on_white ? nvgRGBA(0x0B, 0x14, 0x24, a) : nvgRGBA(0xFF, 0xFF, 0xFF, a);
    };

    // The teal glow.
    NVGpaint glow = nvgRadialGradient(vg, cx, cy, 0.0f, 290.0f, nvgRGBA(0x2E, 0xC4, 0xA6, 23), nvgRGBA(0x2E, 0xC4, 0xA6, 0));
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 290.0f);
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    // Two rings.
    nvgStrokeWidth(vg, 1.5f);
    for (const auto& [r, a] : { std::pair<float, float>{ 226.0f, 0.05f }, { 138.0f, 0.04f } }) {
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgStrokeColor(vg, line(a));
        nvgStroke(vg);
    }

    // The minutes, longer every five.
    for (int i = 0; i < 60; i++) {
        const float ang = (float)i * 2.0f * PI / 60.0f, len = i % 5 == 0 ? 16.0f : 7.0f;
        const float sx = std::sin(ang), sy = -std::cos(ang);
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx + sx * 235.0f, cy + sy * 235.0f);
        nvgLineTo(vg, cx + sx * (235.0f + len), cy + sy * (235.0f + len));
        const float a = i % 5 == 0 ? 0.12f : 0.06f;
        nvgStrokeColor(vg, line(a));
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
    }

    // The day's track, three quarters of it in teal from the top.
    nvgStrokeWidth(vg, 18.0f);
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 182.0f);
    nvgStrokeColor(vg, nvgRGBA(0x1F, 0x33, 0x55, this->light ? 30 : 71));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgArc(vg, cx, cy, 182.0f, -PI / 2.0f, -PI / 2.0f + 1.5f * PI, NVG_CW);
    nvgStrokeColor(vg, nvgRGBA(0x2E, 0xC4, 0xA6, 41));
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);
    nvgLineCap(vg, NVG_BUTT);
}

brls::View* LoadingDial::create()
{
    return new LoadingDial();
}
