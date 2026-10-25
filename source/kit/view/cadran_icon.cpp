// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/cadran_icon.hpp"

#include <cmath>

#include "ui/effects.hpp"
#include "ui/motion.hpp"

namespace
{
constexpr float POP_US = 380000.0f;
}   // namespace

CadranIcon::CadranIcon()
{
    this->color = brls::Application::getTheme()["brls/text"];
    this->setWidth(22);
    this->setHeight(22);
    this->setFocusable(false);
    this->registerStringXMLAttribute("icon", [this](std::string value) { this->setIcon(value); });
    this->registerColorXMLAttribute("color", [this](NVGcolor value) { this->setColor(value); });
}

void CadranIcon::setIcon(const std::string& name) { this->icon = name; }
void CadranIcon::setColor(NVGcolor value) { this->color = value; }
void CadranIcon::pop() { this->pop_since = effects::motion() ? motion::now() : 0; }

static void mark(NVGcontext* vg, float w, float h)
{
    // branding/mark.svg: a play button inside the daily play-time ring (75 %).
    nvgScale(vg, w / 256.0f, h / 256.0f);
    nvgBeginPath(vg);
    nvgCircle(vg, 128, 128, 104);
    nvgFillColor(vg, nvgRGB(0x0B, 0x14, 0x24));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, 128, 128, 74);
    nvgStrokeWidth(vg, 20);
    nvgStrokeColor(vg, nvgRGB(0x1F, 0x33, 0x55));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgArc(vg, 128, 128, 74, -NVG_PI / 2.0f, -NVG_PI / 2.0f + 0.75f * 2.0f * NVG_PI, NVG_CW);
    nvgLineCap(vg, NVG_ROUND);
    nvgStrokeColor(vg, nvgRGB(0x2E, 0xC4, 0xA6));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 110, 98);
    nvgLineTo(vg, 162, 128);
    nvgLineTo(vg, 110, 158);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(0xE8, 0xEE, 0xF7));
    nvgFill(vg);
    nvgStrokeWidth(vg, 10);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStrokeColor(vg, nvgRGB(0xE8, 0xEE, 0xF7));
    nvgStroke(vg);
}

void CadranIcon::draw(NVGcontext* vg, float x, float y, float width, float height,
                      brls::Style style, brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;
    nvgSave(vg);
    nvgTranslate(vg, x, y);
    // Popping: scaled about its middle, from nothing past its size and back.
    if (this->pop_since) {
        const float t = motion::progress(this->pop_since, POP_US);
        if (t >= 1.0f) {
            this->pop_since = 0;
        } else {
            const float scale = motion::back_out(t);
            nvgTranslate(vg, width / 2.0f, height / 2.0f);
            nvgScale(vg, scale, scale);
            nvgTranslate(vg, -width / 2.0f, -height / 2.0f);
        }
    }
    if (this->icon == "mark") {
        mark(vg, width, height);
        nvgRestore(vg);
        return;
    }
    // The boards' icons: a 24-unit box, 2.2-unit round strokes.
    nvgScale(vg, width / 24.0f, height / 24.0f);
    nvgStrokeColor(vg, this->color);
    nvgFillColor(vg, this->color);
    nvgStrokeWidth(vg, 2.2f);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    const std::string& i = this->icon;
    if (i == "today") {
        nvgBeginPath(vg); nvgCircle(vg, 12, 12, 9); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 10, 9); nvgLineTo(vg, 15, 12); nvgLineTo(vg, 10, 15); nvgClosePath(vg);
        nvgFill(vg); nvgStroke(vg);
    } else if (i == "timer") {
        nvgBeginPath(vg); nvgCircle(vg, 12, 12, 9); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 7); nvgLineTo(vg, 12, 12); nvgLineTo(vg, 15, 14); nvgStroke(vg);
    } else if (i == "activity") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 5, 20); nvgLineTo(vg, 5, 12);
        nvgMoveTo(vg, 12, 20); nvgLineTo(vg, 12, 5);
        nvgMoveTo(vg, 19, 20); nvgLineTo(vg, 19, 14);
        nvgStroke(vg);
    } else if (i == "shield") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 12, 3); nvgLineTo(vg, 19, 6); nvgLineTo(vg, 19, 12);
        nvgBezierTo(vg, 19, 16.5f, 16, 19.5f, 12, 21);
        nvgBezierTo(vg, 8, 19.5f, 5, 16.5f, 5, 12);
        nvgLineTo(vg, 5, 6); nvgClosePath(vg); nvgStroke(vg);
    } else if (i == "lock") {
        nvgBeginPath(vg); nvgRoundedRect(vg, 5, 11, 14, 10, 2); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 8, 11); nvgLineTo(vg, 8, 8);
        nvgArc(vg, 12, 8, 4, NVG_PI, 0, NVG_CW); nvgLineTo(vg, 16, 11); nvgStroke(vg);
    } else if (i == "globe") {
        nvgBeginPath(vg); nvgCircle(vg, 12, 12, 9); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 3, 12); nvgLineTo(vg, 21, 12); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 3); nvgBezierTo(vg, 15, 6.5f, 15, 17.5f, 12, 21); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 3); nvgBezierTo(vg, 9, 6.5f, 9, 17.5f, 12, 21); nvgStroke(vg);
    } else if (i == "settings") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 4, 7); nvgLineTo(vg, 13, 7); nvgMoveTo(vg, 19, 7); nvgLineTo(vg, 20, 7);
        nvgMoveTo(vg, 4, 17); nvgLineTo(vg, 7, 17); nvgMoveTo(vg, 13, 17); nvgLineTo(vg, 20, 17);
        nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 16, 7, 2.5f); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 10, 17, 2.5f); nvgStroke(vg);
    } else if (i == "tools") {
        nvgBeginPath(vg); nvgCircle(vg, 16.5f, 7.5f, 4); nvgStroke(vg);
        nvgBeginPath(vg); nvgStrokeWidth(vg, 3); nvgMoveTo(vg, 13.5f, 10.5f); nvgLineTo(vg, 4.5f, 19.5f); nvgStroke(vg);
    } else if (i == "shelf") {
        // Two shelves, books standing on them.
        nvgBeginPath(vg);
        nvgMoveTo(vg, 3, 11); nvgLineTo(vg, 21, 11);
        nvgMoveTo(vg, 3, 21); nvgLineTo(vg, 21, 21);
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgRect(vg, 5.5f, 4, 3, 7); nvgRect(vg, 10.5f, 5.5f, 3, 5.5f);
        nvgRect(vg, 7, 14, 3, 7); nvgRect(vg, 15, 15.5f, 3.5f, 5.5f);
        nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 15.5f, 4.5f); nvgLineTo(vg, 18.5f, 11); nvgStroke(vg);
    } else if (i == "info") {
        nvgBeginPath(vg); nvgCircle(vg, 12, 12, 9); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 11); nvgLineTo(vg, 12, 16.5f); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 12, 8, 1.3f); nvgFill(vg);
    } else if (i == "plus") {
        nvgBeginPath(vg); nvgStrokeWidth(vg, 3);
        nvgMoveTo(vg, 12, 5); nvgLineTo(vg, 12, 19); nvgMoveTo(vg, 5, 12); nvgLineTo(vg, 19, 12); nvgStroke(vg);
    } else if (i == "stop") {
        nvgBeginPath(vg); nvgRoundedRect(vg, 5, 5, 14, 14, 3); nvgFill(vg);
    } else if (i == "eye") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 2, 12);
        nvgBezierTo(vg, 2, 12, 6, 5, 12, 5);
        nvgBezierTo(vg, 18, 5, 22, 12, 22, 12);
        nvgBezierTo(vg, 22, 12, 18, 19, 12, 19);
        nvgBezierTo(vg, 6, 19, 2, 12, 2, 12);
        nvgClosePath(vg); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 12, 12, 3); nvgStroke(vg);
    } else if (i == "sun") {
        // The header's light theme: a disc and eight rays.
        nvgBeginPath(vg); nvgCircle(vg, 12, 12, 4); nvgStroke(vg);
        nvgBeginPath(vg);
        for (int k = 0; k < 8; k++) {
            const float a = (float)k * NVG_PI / 4;
            nvgMoveTo(vg, 12 + std::cos(a) * 7, 12 + std::sin(a) * 7);
            nvgLineTo(vg, 12 + std::cos(a) * 9.5f, 12 + std::sin(a) * 9.5f);
        }
        nvgStroke(vg);
    } else if (i == "tv") {
        // The automatic text size, larger on the TV: a screen on its stand.
        nvgBeginPath(vg); nvgRoundedRect(vg, 3, 5, 18, 12, 2); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 9, 21); nvgLineTo(vg, 15, 21); nvgMoveTo(vg, 12, 17); nvgLineTo(vg, 12, 21);
        nvgStroke(vg);
    } else if (i == "text-normal" || i == "text-large") {
        // A text size: an A, small or large, on the same baseline (as the
        // header's button).
        nvgFontFaceId(vg, brls::Application::getDefaultFont());
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
        nvgFontSize(vg, i == "text-large" ? 28 : 18);
        nvgText(vg, 12, 20.5f, "A", nullptr);
    } else if (i == "moon") {
        nvgBeginPath(vg);
        nvgArc(vg, 12, 12, 9, 0.11f, 4.60f, NVG_CW);
        nvgArc(vg, 16, 8, 7.07f, 3.93f, 0.785f, NVG_CCW);
        nvgClosePath(vg); nvgStroke(vg);
    } else if (i == "pause") {
        nvgBeginPath(vg); nvgMoveTo(vg, 9, 5); nvgLineTo(vg, 9, 19); nvgMoveTo(vg, 15, 5); nvgLineTo(vg, 15, 19); nvgStroke(vg);
    } else if (i == "list") {
        nvgBeginPath(vg); nvgMoveTo(vg, 4, 6); nvgLineTo(vg, 20, 6); nvgMoveTo(vg, 4, 12); nvgLineTo(vg, 20, 12);
        nvgMoveTo(vg, 4, 18); nvgLineTo(vg, 14, 18); nvgStroke(vg);
    } else if (i == "lines") {
        nvgBeginPath(vg); nvgMoveTo(vg, 3, 12); nvgLineTo(vg, 11, 12); nvgMoveTo(vg, 3, 6); nvgLineTo(vg, 21, 6);
        nvgMoveTo(vg, 3, 18); nvgLineTo(vg, 15, 18); nvgStroke(vg);
    } else if (i == "download") {
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 3); nvgLineTo(vg, 12, 15); nvgMoveTo(vg, 7, 10); nvgLineTo(vg, 12, 15); nvgLineTo(vg, 17, 10);
        nvgMoveTo(vg, 4, 20); nvgLineTo(vg, 20, 20); nvgStroke(vg);
    } else if (i == "chevron-down") {
        nvgBeginPath(vg); nvgStrokeWidth(vg, 2.5f); nvgMoveTo(vg, 6, 9); nvgLineTo(vg, 12, 15); nvgLineTo(vg, 18, 9); nvgStroke(vg);
    } else if (i == "chevron-left") {
        nvgBeginPath(vg); nvgStrokeWidth(vg, 2.5f); nvgMoveTo(vg, 15, 6); nvgLineTo(vg, 9, 12); nvgLineTo(vg, 15, 18); nvgStroke(vg);
    } else if (i == "chevron-right") {
        nvgBeginPath(vg); nvgStrokeWidth(vg, 2.5f); nvgMoveTo(vg, 9, 6); nvgLineTo(vg, 15, 12); nvgLineTo(vg, 9, 18); nvgStroke(vg);
    } else if (i == "caret-up") {
        nvgBeginPath(vg); nvgStrokeWidth(vg, 3); nvgMoveTo(vg, 4, 15); nvgLineTo(vg, 12, 7); nvgLineTo(vg, 20, 15); nvgStroke(vg);
    } else if (i == "caret-down") {
        nvgBeginPath(vg); nvgStrokeWidth(vg, 3); nvgMoveTo(vg, 4, 9); nvgLineTo(vg, 12, 17); nvgLineTo(vg, 20, 9); nvgStroke(vg);
    } else if (i == "check") {
        nvgBeginPath(vg); nvgStrokeWidth(vg, 3);
        nvgMoveTo(vg, 4, 12); nvgLineTo(vg, 9, 17); nvgLineTo(vg, 20, 7); nvgStroke(vg);
    } else if (i == "key") {
        nvgBeginPath(vg); nvgCircle(vg, 7.5f, 15.5f, 4); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 10.5f, 12.5f); nvgLineTo(vg, 20, 3);
        nvgMoveTo(vg, 16.5f, 6.5f); nvgLineTo(vg, 19.5f, 9.5f); nvgMoveTo(vg, 14, 9); nvgLineTo(vg, 16, 11); nvgStroke(vg);
    } else if (i == "unlock") {
        nvgBeginPath(vg); nvgRoundedRect(vg, 5, 11, 14, 10, 2); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 8, 11); nvgLineTo(vg, 8, 8);
        nvgArc(vg, 12, 8, 4, NVG_PI, NVG_PI * 1.82f, NVG_CW); nvgStroke(vg);
    } else if (i == "phone") {
        nvgBeginPath(vg); nvgRoundedRect(vg, 7, 2, 10, 20, 2.5f); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 12, 18, 1.1f); nvgFill(vg);
    } else if (i == "trash") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 4, 7); nvgLineTo(vg, 20, 7);
        nvgMoveTo(vg, 9, 7); nvgLineTo(vg, 9, 4); nvgLineTo(vg, 15, 4); nvgLineTo(vg, 15, 7);
        nvgMoveTo(vg, 6, 7); nvgLineTo(vg, 7, 20); nvgLineTo(vg, 17, 20); nvgLineTo(vg, 18, 7);
        nvgMoveTo(vg, 10, 11); nvgLineTo(vg, 10, 16); nvgMoveTo(vg, 14, 11); nvgLineTo(vg, 14, 16);
        nvgStroke(vg);
    } else if (i == "server") {
        nvgBeginPath(vg); nvgRoundedRect(vg, 3, 4, 18, 7, 2); nvgRoundedRect(vg, 3, 13, 18, 7, 2); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 7, 7.5f, 1.1f); nvgCircle(vg, 7, 16.5f, 1.1f); nvgFill(vg);
    } else if (i == "pin") {
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 21);
        nvgBezierTo(vg, 8, 17, 5, 13.5f, 5, 10); nvgArc(vg, 12, 10, 7, NVG_PI, 2.0f * NVG_PI, NVG_CW);
        nvgBezierTo(vg, 19, 13.5f, 16, 17, 12, 21); nvgClosePath(vg); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 12, 10, 2.6f); nvgStroke(vg);
    } else if (i == "refresh" || i == "history" || i == "restore") {
        // An arc around the clock face, its arrow at the top: forward
        // (refresh) or back (history, restore); history adds the hands.
        const bool back = i != "refresh";
        nvgBeginPath(vg);
        if (back) {
            nvgArc(vg, 12, 12, 9, NVG_PI, -NVG_PI / 2.0f, NVG_CCW);
            nvgBezierTo(vg, 9.5f, 3, 7.1f, 4, 5.3f, 5.7f); nvgLineTo(vg, 3, 8);
            nvgMoveTo(vg, 3, 3); nvgLineTo(vg, 3, 8); nvgLineTo(vg, 8, 8);
        } else {
            nvgArc(vg, 12, 12, 9, 0, 1.5f * NVG_PI, NVG_CW);
            nvgBezierTo(vg, 14.5f, 3, 16.9f, 4, 18.7f, 5.7f); nvgLineTo(vg, 21, 8);
            nvgMoveTo(vg, 21, 3); nvgLineTo(vg, 21, 8); nvgLineTo(vg, 16, 8);
        }
        if (i == "history") { nvgMoveTo(vg, 12, 7); nvgLineTo(vg, 12, 12); nvgLineTo(vg, 15.5f, 14); }
        nvgStroke(vg);
    } else if (i == "save") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 5, 3); nvgLineTo(vg, 16, 3); nvgLineTo(vg, 21, 8); nvgLineTo(vg, 21, 19);
        nvgQuadTo(vg, 21, 21, 19, 21); nvgLineTo(vg, 5, 21); nvgQuadTo(vg, 3, 21, 3, 19); nvgLineTo(vg, 3, 5);
        nvgQuadTo(vg, 3, 3, 5, 3); nvgClosePath(vg);
        nvgMoveTo(vg, 7, 21); nvgLineTo(vg, 7, 14); nvgLineTo(vg, 17, 14); nvgLineTo(vg, 17, 21);
        nvgMoveTo(vg, 7, 3); nvgLineTo(vg, 7, 8); nvgLineTo(vg, 15, 8);
        nvgStroke(vg);
    } else if (i == "upload") {
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 15); nvgLineTo(vg, 12, 3); nvgMoveTo(vg, 7, 8); nvgLineTo(vg, 12, 3); nvgLineTo(vg, 17, 8);
        nvgMoveTo(vg, 4, 20); nvgLineTo(vg, 20, 20); nvgStroke(vg);
    } else if (i == "language") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 5, 8); nvgLineTo(vg, 11, 14);
        nvgMoveTo(vg, 4, 14); nvgLineTo(vg, 10, 8); nvgLineTo(vg, 12, 5);
        nvgMoveTo(vg, 2, 5); nvgLineTo(vg, 14, 5); nvgMoveTo(vg, 7, 2); nvgLineTo(vg, 8, 2);
        nvgMoveTo(vg, 22, 22); nvgLineTo(vg, 17, 12); nvgLineTo(vg, 12, 22);
        nvgMoveTo(vg, 14, 18); nvgLineTo(vg, 20, 18);
        nvgStroke(vg);
    } else if (i == "contrast") {
        nvgBeginPath(vg); nvgCircle(vg, 12, 12, 9); nvgStroke(vg);
        nvgBeginPath(vg); nvgArc(vg, 12, 12, 9, -NVG_PI / 2.0f, NVG_PI / 2.0f, NVG_CW); nvgClosePath(vg); nvgFill(vg);
    } else if (i == "home") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 3, 10); nvgLineTo(vg, 12, 3); nvgLineTo(vg, 21, 10); nvgLineTo(vg, 21, 20);
        nvgQuadTo(vg, 21, 21, 20, 21); nvgLineTo(vg, 4, 21); nvgQuadTo(vg, 3, 21, 3, 20); nvgClosePath(vg);
        nvgMoveTo(vg, 9, 21); nvgLineTo(vg, 9, 14); nvgLineTo(vg, 15, 14); nvgLineTo(vg, 15, 21);
        nvgStroke(vg);
    } else if (i == "bolt") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 13, 2); nvgLineTo(vg, 4, 14); nvgLineTo(vg, 12, 14); nvgLineTo(vg, 11, 22);
        nvgLineTo(vg, 20, 10); nvgLineTo(vg, 12, 10); nvgClosePath(vg); nvgStroke(vg);
    } else if (i == "sparkle") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 12, 3); nvgLineTo(vg, 13.8f, 10.2f); nvgLineTo(vg, 21, 12); nvgLineTo(vg, 13.8f, 13.8f);
        nvgLineTo(vg, 12, 21); nvgLineTo(vg, 10.2f, 13.8f); nvgLineTo(vg, 3, 12); nvgLineTo(vg, 10.2f, 10.2f);
        nvgClosePath(vg); nvgStroke(vg);
    } else if (i == "bell") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 6, 9); nvgArc(vg, 12, 9, 6, NVG_PI, 2.0f * NVG_PI, NVG_CW);
        nvgBezierTo(vg, 18, 15, 21, 17, 21, 17); nvgLineTo(vg, 3, 17); nvgBezierTo(vg, 3, 17, 6, 15, 6, 9);
        nvgMoveTo(vg, 10, 20.5f); nvgBezierTo(vg, 11, 21.8f, 13, 21.8f, 14, 20.5f);
        nvgStroke(vg);
    } else if (i == "chip") {
        nvgBeginPath(vg); nvgRoundedRect(vg, 6, 6, 12, 12, 2); nvgStroke(vg);
        nvgBeginPath(vg); nvgRect(vg, 9.5f, 9.5f, 5, 5); nvgFill(vg);
        nvgBeginPath(vg);
        for (float p : { 9.0f, 15.0f }) {
            nvgMoveTo(vg, p, 2); nvgLineTo(vg, p, 6); nvgMoveTo(vg, p, 18); nvgLineTo(vg, p, 22);
            nvgMoveTo(vg, 2, p); nvgLineTo(vg, 6, p); nvgMoveTo(vg, 18, p); nvgLineTo(vg, 22, p);
        }
        nvgStroke(vg);
    } else if (i == "code") {
        nvgBeginPath(vg); nvgMoveTo(vg, 16, 18); nvgLineTo(vg, 22, 12); nvgLineTo(vg, 16, 6);
        nvgMoveTo(vg, 8, 6); nvgLineTo(vg, 2, 12); nvgLineTo(vg, 8, 18); nvgStroke(vg);
    } else if (i == "heart") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 12, 21); nvgLineTo(vg, 5, 14); nvgBezierTo(vg, 3.5f, 12.5f, 2, 10.8f, 2, 8.5f);
        nvgBezierTo(vg, 2, 5.5f, 4.5f, 3, 7.5f, 3); nvgBezierTo(vg, 9.3f, 3, 10.5f, 3.5f, 12, 5);
        nvgBezierTo(vg, 13.5f, 3.5f, 14.7f, 3, 16.5f, 3); nvgBezierTo(vg, 19.5f, 3, 22, 5.5f, 22, 8.5f);
        nvgBezierTo(vg, 22, 10.8f, 20.5f, 12.5f, 19, 14); nvgClosePath(vg); nvgStroke(vg);
    } else if (i == "folder") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 4, 20); nvgLineTo(vg, 20, 20); nvgQuadTo(vg, 22, 20, 22, 18); nvgLineTo(vg, 22, 9);
        nvgQuadTo(vg, 22, 7, 20, 7); nvgLineTo(vg, 12, 7); nvgLineTo(vg, 10, 4); nvgLineTo(vg, 4, 4);
        nvgQuadTo(vg, 2, 4, 2, 6); nvgLineTo(vg, 2, 18); nvgQuadTo(vg, 2, 20, 4, 20); nvgClosePath(vg); nvgStroke(vg);
    } else if (i == "file") {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 14, 2); nvgLineTo(vg, 6, 2); nvgQuadTo(vg, 4, 2, 4, 4); nvgLineTo(vg, 4, 20);
        nvgQuadTo(vg, 4, 22, 6, 22); nvgLineTo(vg, 18, 22); nvgQuadTo(vg, 20, 22, 20, 20); nvgLineTo(vg, 20, 8); nvgClosePath(vg);
        nvgMoveTo(vg, 14, 2); nvgLineTo(vg, 14, 8); nvgLineTo(vg, 20, 8);
        nvgMoveTo(vg, 8, 13); nvgLineTo(vg, 16, 13); nvgMoveTo(vg, 8, 17); nvgLineTo(vg, 14, 17);
        nvgStroke(vg);
    } else if (i == "flag") {
        nvgBeginPath(vg); nvgMoveTo(vg, 4, 22); nvgLineTo(vg, 4, 3);
        nvgQuadTo(vg, 8, 1, 12, 3); nvgQuadTo(vg, 16, 5, 20, 3); nvgLineTo(vg, 20, 15);
        nvgQuadTo(vg, 16, 17, 12, 15); nvgQuadTo(vg, 8, 13, 4, 15); nvgStroke(vg);
    } else if (i == "gamepad") {
        nvgBeginPath(vg); nvgRoundedRect(vg, 2, 7, 20, 11, 5.5f); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 6, 12.5f); nvgLineTo(vg, 10, 12.5f); nvgMoveTo(vg, 8, 10.5f); nvgLineTo(vg, 8, 14.5f); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 15.5f, 13.5f, 1.1f); nvgCircle(vg, 18, 11, 1.1f); nvgFill(vg);
    } else if (i == "calendar") {
        nvgBeginPath(vg); nvgRoundedRect(vg, 3, 5, 18, 16, 2); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 3, 10); nvgLineTo(vg, 21, 10);
        nvgMoveTo(vg, 8, 3); nvgLineTo(vg, 8, 7); nvgMoveTo(vg, 16, 3); nvgLineTo(vg, 16, 7); nvgStroke(vg);
    } else if (i == "alert") {
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 3); nvgLineTo(vg, 22, 20); nvgLineTo(vg, 2, 20); nvgClosePath(vg); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 12, 9); nvgLineTo(vg, 12, 14); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 12, 17, 1.1f); nvgFill(vg);
    } else if (i == "pencil") {
        nvgBeginPath(vg); nvgMoveTo(vg, 4, 20); nvgLineTo(vg, 8, 20); nvgLineTo(vg, 19, 9); nvgLineTo(vg, 15, 5);
        nvgLineTo(vg, 4, 16); nvgClosePath(vg); nvgMoveTo(vg, 13, 7); nvgLineTo(vg, 17, 11); nvgStroke(vg);
    } else if (i == "pencil-off") {   // read-only: the pencil, crossed out
        nvgBeginPath(vg); nvgMoveTo(vg, 4, 20); nvgLineTo(vg, 8, 20); nvgLineTo(vg, 19, 9); nvgLineTo(vg, 15, 5);
        nvgLineTo(vg, 4, 16); nvgClosePath(vg); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 3, 3); nvgLineTo(vg, 21, 21); nvgStroke(vg);
    } else if (i == "tag") {
        nvgBeginPath(vg); nvgMoveTo(vg, 3, 3); nvgLineTo(vg, 11, 3); nvgLineTo(vg, 21, 13); nvgLineTo(vg, 13, 21);
        nvgLineTo(vg, 3, 11); nvgClosePath(vg); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, 7.5f, 7.5f, 1.5f); nvgFill(vg);
    } else if (i == "user") {
        nvgBeginPath(vg); nvgCircle(vg, 12, 8, 4); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, 4, 21); nvgQuadTo(vg, 4, 14, 12, 14); nvgQuadTo(vg, 20, 14, 20, 21); nvgStroke(vg);
    }
    nvgRestore(vg);
}

brls::View* CadranIcon::create() { return new CadranIcon(); }
