// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/banner_card.hpp"

#include <algorithm>

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/effects.hpp>
#include <borealis/cadran/ui/motion.hpp>

namespace
{
constexpr float RADIUS = 16;
constexpr float FADE_US = 240000.0f;   // a picture's arrival
}   // namespace

BannerCard::BannerCard(float width, float height)
{
    this->setAxis(brls::Axis::COLUMN);
    this->setJustifyContent(brls::JustifyContent::FLEX_END);
    this->setAlignItems(brls::AlignItems::FLEX_START);
    this->setWidth(width);
    this->setHeight(height);
    this->setShrink(0);
    this->setPadding(18, 22, 18, 22);
    this->setMargins(8, 8, 8, 8);
    this->setCornerRadius(RADIUS);
    this->setHighlightCornerRadius(RADIUS + 2);
    this->setFocusable(true);

    this->kicker = new brls::Box(brls::Axis::ROW);
    this->kicker->setHeight(26);
    this->kicker->setAlignItems(brls::AlignItems::CENTER);
    this->kicker->setPaddingLeft(10);
    this->kicker->setPaddingRight(10);
    this->kicker->setCornerRadius(13);
    this->kicker->setMarginBottom(8);
    this->kicker->setVisibility(brls::Visibility::GONE);
    this->kicker_text = cadran::kit::label("", 14, "cadran/navy");
    this->kicker_text->setSingleLine(true);
    this->kicker->addView(this->kicker_text);
    this->addView(this->kicker);

    this->title = cadran::kit::display("", 24);
    this->title->setSingleLine(true);
    this->addView(this->title);
    this->line = cadran::kit::label("", 15, "cadran/text2");
    this->line->setSingleLine(true);
    this->line->setMarginTop(4);
    this->addView(this->line);
}

BannerCard::~BannerCard()
{
    if (this->texture) nvgDeleteImage(brls::Application::getNVGContext(), this->texture);
}

void BannerCard::set_title(const std::string& text)
{
    this->title->setText(text);
}

void BannerCard::set_line(const std::string& text)
{
    this->line->setText(text);
    this->line->setVisibility(text.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
}

void BannerCard::set_kicker(const std::string& text, const char* ink)
{
    this->kicker->setVisibility(text.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    if (text.empty()) return;
    this->kicker_text->setText(text);
    this->kicker->setBackgroundColor(cadran::kit::color(ink));
}

void BannerCard::set_picture(int tex, int w, int h)
{
    if (!tex) return;
    if (this->texture) nvgDeleteImage(brls::Application::getNVGContext(), this->texture);
    this->texture = tex;
    this->picture_w = w;
    this->picture_h = h;
    this->since = effects::motion() ? motion::now() : 0;
}

void BannerCard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                      brls::FrameContext* ctx)
{
    brls::Theme theme = brls::Application::getTheme();
    const float shown = this->getAlpha();
    // The well: the page's two surfaces, until the picture comes.
    NVGcolor top = theme["cadran/surf2"], bottom = theme["cadran/surf"];
    top.a *= shown;
    bottom.a *= shown;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, RADIUS);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + width, y + height, top, bottom));
    nvgFill(vg);

    if (this->texture && this->picture_w > 0 && this->picture_h > 0) {
        float fade = 1.0f;
        if (this->since) {
            fade = motion::ease_out(motion::progress(this->since, FADE_US));
            if (fade >= 1.0f) this->since = 0;
        }
        // Cropped to fill the card, centred.
        const float scale = std::max(width / (float)this->picture_w, height / (float)this->picture_h);
        const float w = (float)this->picture_w * scale, h = (float)this->picture_h * scale;
        const NVGpaint paint = nvgImagePattern(vg, x + (width - w) / 2, y + (height - h) / 2, w, h, 0, this->texture,
                                               shown * fade);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, width, height, RADIUS);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }

    // Its lower part fading into the page, for the words over it.
    NVGcolor solid = theme["brls/background"];
    solid.a = 0.94f * shown;
    NVGcolor clear = solid;
    clear.a = 0.0f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, RADIUS);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y + height * 0.3f, x, y + height, clear, solid));
    nvgFill(vg);

    brls::Box::draw(vg, x, y, width, height, style, ctx);
}
