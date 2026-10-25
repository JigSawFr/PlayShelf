// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/cadran_digits.hpp"

#include <algorithm>
#include <cmath>

#include "ui/effects.hpp"
#include "ui/motion.hpp"

namespace
{
constexpr float ROLL_US = 240000.0f;   // a digit's roll, in µs
}   // namespace

CadranDigits::CadranDigits()
{
    this->color = brls::Application::getTheme()["brls/text"];
    this->font = brls::Application::getFont("cadran/digits");
    if (this->font == brls::FONT_INVALID) this->font = brls::Application::getDefaultFont();
    this->setFocusable(false);
    this->registerStringXMLAttribute("text", [this](std::string value) { this->setText(value); });
    this->registerFloatXMLAttribute("fontSize", [this](float value) { this->setFontSize(value); });
    this->registerColorXMLAttribute("textColor", [this](NVGcolor value) { this->setTextColor(value); });
    this->registerFloatXMLAttribute("fitWidth", [this](float value) { this->setFitWidth(value); });
    this->registerBoolXMLAttribute("fitParent", [this](bool value) { this->setFitParent(value); });
    this->registerBoolXMLAttribute("followsTextSize", [this](bool value) { this->setFollowsTextSize(value); });
    this->measure();
}

void CadranDigits::setText(const std::string& value)
{
    // A text shown, replaced by another: the old one rolls out as the new
    // one rolls in (not the first text, nor a placeholder's dash).
    if (value != this->text && this->rolls && this->drawn && !this->text.empty() && this->text != "\u2014" && effects::motion()) {
        this->leaving      = this->text;
        this->leaving_size = this->drawSize;
        this->roll_since   = motion::now();
    }
    this->text = value;
    this->measure();
}

void CadranDigits::setRolls(bool on) { this->rolls = on; }

void CadranDigits::setFontSize(float value)
{
    this->fontSize = value;
    this->measure();
}

void CadranDigits::setTextColor(NVGcolor value) { this->color = value; }

void CadranDigits::setFollowsTextSize(bool follows)
{
    this->followsTextSize = follows;
    this->measure();
}

void CadranDigits::setFitWidth(float width)
{
    this->fitWidth = width;
    this->measure();
}

void CadranDigits::setFitParent(bool fit) { this->fitParent = fit; }

void CadranDigits::setSmallUnits(bool small)
{
    this->smallUnits = small;
    this->measure();
}

namespace
{
constexpr float UNIT_SHARE = 0.5f;   // a word's size beside the figures
}   // namespace

std::vector<CadranDigits::Run> CadranDigits::runs(const std::string& of) const
{
    std::vector<Run> out;
    const bool any = this->smallUnits && std::any_of(of.begin(), of.end(),
                                                     [](char c) { return c >= '0' && c <= '9'; });
    if (!any) return { { of, true } };
    for (char c : of) {
        const bool figure = c >= '0' && c <= '9';
        if (out.empty() || out.back().figure != figure) out.push_back({ "", figure });
        out.back().text += c;   // a word's UTF-8 bytes stay together: none is a figure
    }
    return out;
}

float CadranDigits::advance(NVGcontext* vg, float size) const
{
    float total = 0.0f, bounds[4] = { 0, 0, 0, 0 };
    for (const Run& run : this->runs(this->text)) {
        const float s = run.figure ? size : size * UNIT_SHARE;
        nvgFontSize(vg, s);
        nvgTextLetterSpacing(vg, -0.02f * s);
        total += nvgTextBounds(vg, 0, 0, run.text.c_str(), nullptr, bounds);
    }
    nvgTextLetterSpacing(vg, 0);
    return total;
}

void CadranDigits::measure()
{
    NVGcontext* vg = brls::Application::getNVGContext();
    nvgFontFaceId(vg, this->font);
    const float size = this->fontSize * (this->followsTextSize ? brls::Label::getFontScale() : 1.0f);
    float width = this->advance(vg, size);
    this->drawSize = size;
    if (this->fitWidth > 0.0f && width > this->fitWidth) {
        this->drawSize = std::max(10.0f, size * this->fitWidth / width);
        width = this->advance(vg, this->drawSize);
    }
    this->setWidth(std::ceil(width));
    this->setHeight(std::ceil(size * 1.15f));
    this->invalidate();
}

void CadranDigits::write(NVGcontext* vg, const std::string& what, float size, float x, float y, float height) const
{
    const std::vector<Run> parts = this->runs(what);
    if (parts.size() == 1) {
        nvgFontSize(vg, size);
        nvgTextLetterSpacing(vg, -0.02f * size);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, x, y + height / 2.0f, what.c_str(), nullptr);
    } else {
        // One baseline, the figures centred as a single-size text would be.
        float ascender = 0.0f, descender = 0.0f, line = 0.0f;
        nvgFontSize(vg, size);
        nvgTextMetrics(vg, &ascender, &descender, &line);
        const float baseline = y + height / 2.0f + (ascender + descender) / 2.0f;
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
        float at = x;
        for (const Run& run : parts) {
            const float s = run.figure ? size : size * UNIT_SHARE;
            nvgFontSize(vg, s);
            nvgTextLetterSpacing(vg, -0.02f * s);
            at = nvgText(vg, at, baseline, run.text.c_str(), nullptr);
        }
    }
    nvgTextLetterSpacing(vg, 0);
}

void CadranDigits::draw(NVGcontext* vg, float x, float y, float width, float height,
                        brls::Style style, brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;
    // The parent's width known once laid out, or changed: the text fitted
    // to it, drawn at that size from the next frame.
    if (this->fitParent && this->getParent()) {
        const float room = std::floor(this->getParent()->getWidth());
        if (room > 0.0f && room != this->fitWidth) this->setFitWidth(room);
    }
    this->drawn = true;
    nvgFontFaceId(vg, this->font);
    const float t = motion::progress(this->roll_since, ROLL_US);
    if (t >= 1.0f) {
        this->roll_since = 0;
        nvgFillColor(vg, this->color);
        this->write(vg, this->text, this->drawSize, x, y, height);
        return;
    }
    // Rolling: the old text up and out, the new one in from below, both
    // within the line (a little above and below it, for the descenders).
    const float e = motion::ease_out(t);
    const float travel = height * 0.6f;
    nvgSave(vg);
    nvgIntersectScissor(vg, x - 4.0f, y - 2.0f, std::max(width, this->getWidth()) + 8.0f, height + 4.0f);
    NVGcolor old_ink = this->color, new_ink = this->color;
    old_ink.a *= 1.0f - e;
    new_ink.a *= e;
    nvgFillColor(vg, old_ink);
    this->write(vg, this->leaving, this->leaving_size, x, y - travel * e, height);
    nvgFillColor(vg, new_ink);
    this->write(vg, this->text, this->drawSize, x, y + travel * (1.0f - e), height);
    nvgRestore(vg);
}

brls::View* CadranDigits::create() { return new CadranDigits(); }
