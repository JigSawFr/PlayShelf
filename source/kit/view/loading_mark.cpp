// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/loading_mark.hpp"

#include <cmath>
#include <memory>
#include <string>

#include <aurore/core/theme_image.hpp>

#include "util/paths.hpp"
#ifndef __SWITCH__
#include <cstdlib>
#include "core/sim_env.h"
#endif

namespace
{
// Aurore's aurora (branding/*.svg): its teal and its violet.
const NVGcolor AURORA_TEAL   = nvgRGB(0x00, 0xFF, 0xCC);
const NVGcolor AURORA_VIOLET = nvgRGB(0x8A, 0x5C, 0xFF);

constexpr float PI = 3.14159265f;
// One breath of the glow, teal to violet and back, in seconds.
constexpr float BREATH_S = 3.2f;

NVGcolor with_alpha(NVGcolor c, float a)
{
    c.a = a;
    return c;
}

// The app's icon without its name (images/icon-512.png, img/app_icon.png in
// the resources; icon.jpg carries the name for the HOME menu) as a texture,
// made once; 0 when it cannot be read.
int icon_texture(NVGcontext* vg)
{
    static int texture = -1;
    if (texture < 0) {
        std::string bytes;
        texture = paths::read_file(AURORE_ASSET("img/app_icon.png"), bytes) && !bytes.empty()
                      ? nvgCreateImageMem(vg, NVG_IMAGE_GENERATE_MIPMAPS, (unsigned char*)bytes.data(), (int)bytes.size())
                      : 0;
    }
    return texture;
}
// The aurora's three curtains (branding/aurora-loader.svg of boreale, the loader
// of the brand's other screens) swaying behind the mark: a 3 s loop, the
// curtains a third of a loop apart, the gradient from teal to violet.
constexpr float CURTAIN_LOOP_S = 3.0f;

void draw_curtains(NVGcontext* vg, float cx, float cy, float size, double seconds, float alpha)
{
    const float half = size * 1.3f;                    // wider than the mark: they show at its sides
    const float x0 = cx - half, x1 = cx + half, w = 2.0f * half;
    const float sway = size * 0.16f;
    const float gap = size * 0.094f;
    const float opacity[3] = { 0.45f, 0.8f, 0.45f };
    for (int k = 0; k < 3; ++k) {
        const float phase = (float)std::fmod(seconds / CURTAIN_LOOP_S + k / 3.0, 1.0);
        const float s = sway * std::cos(phase * 2.0f * PI);
        const float y = cy + (k - 1) * gap;
        const NVGpaint paint = nvgLinearGradient(vg, x0, cy, x1, cy, with_alpha(AURORA_TEAL, opacity[k] * alpha),
                                                 with_alpha(AURORA_VIOLET, opacity[k] * alpha));
        nvgBeginPath(vg);
        nvgMoveTo(vg, x0, y);
        nvgBezierTo(vg, x0 + w * 0.195f, y - s, x0 + w * 0.352f, y + s, cx, y);
        nvgBezierTo(vg, x0 + w * 0.648f, y - s, x0 + w * 0.805f, y - s, x1, y);
        nvgStrokeWidth(vg, size * 0.086f);
        nvgLineCap(vg, NVG_ROUND);
        nvgStrokePaint(vg, paint);
        nvgStroke(vg);
    }
}
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
                       aurore::Style style, aurore::FrameContext* ctx)
{
    (void)style;
    (void)ctx;
    const float size = std::fmin(width, height);
    const float cx = x + width / 2.0f, cy = y + height / 2.0f;

    // The aurora's glow behind the tile, past the view's bounds, breathing
    // from its teal to its violet while the app starts.
    float violet = 0.35f;
    if (!this->still) {
        const double t = (double)aurore::getCPUTimeUsec() / 1e6;
        violet = 0.5f - 0.5f * std::cos((float)std::fmod(t / BREATH_S, 1.0) * 2.0f * PI);
    }
    const struct { NVGcolor colour; float share; } glows[] = { { AURORA_TEAL, 1.0f - violet }, { AURORA_VIOLET, violet } };
    for (const auto& g : glows) {
        NVGpaint glow = nvgRadialGradient(vg, cx, cy, size * 0.35f, size * 1.25f, with_alpha(g.colour, 0.22f * g.share),
                                          with_alpha(g.colour, 0.0f));
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, size * 1.25f);
        nvgFillPaint(vg, glow);
        nvgFill(vg);
    }
    // The curtains too, where the screen moves (the smoke test's stands still).
    if (!this->still) draw_curtains(vg, cx, cy, size, (double)aurore::getCPUTimeUsec() / 1e6, this->getAlpha());

    // The theme's logo when it has one (`logo =`, loaded), fitted in the
    // icon's square as Cadran fits it in its mark; asked at each frame, so
    // a theme chosen live shows its own at once.
    const std::shared_ptr<aurore::ThemeImage> logo = aurore::Theme::logo();
    if (logo && logo->draw(vg, cx - size / 2.0f, cy - size / 2.0f, size, size, aurore::BackgroundFit::CONTAIN, nullptr,
                           this->getAlpha()))
        return;

    // Else the icon, rounded as the HOME menu rounds it.
    const int texture = icon_texture(vg);
    if (!texture) return;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, cx - size / 2.0f, cy - size / 2.0f, size, size, size * 0.22f);
    nvgFillPaint(vg, nvgImagePattern(vg, cx - size / 2.0f, cy - size / 2.0f, size, size, 0.0f, texture, 1.0f));
    nvgFill(vg);
}

aurore::View* LoadingMark::create()
{
    return new LoadingMark();
}
