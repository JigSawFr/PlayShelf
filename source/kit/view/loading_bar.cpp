// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/loading_bar.hpp"

#include <algorithm>

namespace
{
// The brand palette (branding/*.svg).
const NVGcolor TRACK      = nvgRGB(0x1F, 0x33, 0x55);
const NVGcolor TEAL       = nvgRGB(0x2E, 0xC4, 0xA6);
const NVGcolor TEAL_LIGHT = nvgRGB(0x9A, 0xF0, 0xDC);

// How long the fill takes to reach a new value, in µs.
constexpr float EASE_US = 350000.0f;

float eased(float t)   // ease-out cubic, 0..1
{
    const float r = 1.0f - t;
    return 1.0f - r * r * r;
}
}   // namespace

LoadingBar::LoadingBar()
{
    this->setFocusable(false);
}

void LoadingBar::setProgress(float value)
{
    this->moveTo(value, EASE_US, false);
}

void LoadingBar::fillOver(float value, brls::Time duration_us)
{
    this->moveTo(value, (float)duration_us, true);
}

void LoadingBar::reset()
{
    this->from = this->to = 0.0f;
    this->duration = 1.0f;
    this->since = brls::getCPUTimeUsec();
}

void LoadingBar::moveTo(float value, float duration_us, bool at_steady_pace)
{
    // From where the fill is drawn now, not from the last value asked.
    const brls::Time now = brls::getCPUTimeUsec();
    this->from     = this->drawn(now);
    this->to       = std::clamp(value, 0.0f, 1.0f);
    this->since    = now;
    this->duration = std::max(duration_us, 1.0f);
    this->steady   = at_steady_pace;
}

float LoadingBar::drawn(brls::Time now) const
{
    if (!this->since) return this->to;
    const float t = std::min(1.0f, (float)(now - this->since) / this->duration);
    return this->from + (this->to - this->from) * (this->steady ? t : eased(t));
}

void LoadingBar::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style style, brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;
    const float radius = height / 2.0f;
    brls::Theme theme = brls::Application::getTheme();
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, radius);
    nvgFillColor(vg, this->a(this->brand ? TRACK : theme["cadran/well"]));
    nvgFill(vg);

    const float value = this->drawn(brls::getCPUTimeUsec());
    if (value <= 0.0f) return;
    const float w = std::max(height, width * value);
    // Teal, lighter toward its end: the end that moves.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, height, radius);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + w, y, this->a(this->brand ? TEAL : theme["cadran/teal_fill"]),
                                       this->a(this->brand ? TEAL_LIGHT : theme["brls/highlight/color2"])));
    nvgFill(vg);
}

brls::View* LoadingBar::create()
{
    return new LoadingBar();
}
