// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/game_tile.hpp"

#include <algorithm>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/effects.hpp>
#include <aurore/cadran/ui/motion.hpp>

#include "ui/ui.hpp"
#include "view/cover_placeholder.hpp"

namespace
{
constexpr float MARGIN = 8;
constexpr float FADE_US = 180000.0f;   // a cover's arrival
constexpr float PADDING = 8;
constexpr float COVER = GameTile::WIDTH - 2 * MARGIN - 2 * PADDING;   // 144
}   // namespace

GameTile::GameTile()
{
    this->setAxis(aurore::Axis::COLUMN);
    this->setAlignItems(aurore::AlignItems::STRETCH);
    this->setWidth(WIDTH - 2 * MARGIN);
    this->setMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    this->setPadding(PADDING, PADDING, PADDING, PADDING);
    this->setCornerRadius(10);
    this->setHighlightCornerRadius(12);
    this->setFocusable(true);

    this->cover = new aurore::Box(aurore::Axis::ROW);
    this->cover->setWidth(COVER);
    this->cover->setHeight(COVER);
    this->cover->setCornerRadius(8);
    this->cover->setThemeColor("backgroundColor", "cadran/well");
    this->cover->setAlignItems(aurore::AlignItems::CENTER);
    this->cover->setJustifyContent(aurore::JustifyContent::CENTER);
    this->placeholder = new CoverPlaceholder(COVER, 56, 8);
    this->cover->addView(this->placeholder);
    this->icon = new aurore::Image();
    this->icon->setWidth(COVER);
    this->icon->setHeight(COVER);
    this->icon->setCornerRadius(8);
    this->icon->setScalingType(aurore::ImageScalingType::FIT);
    this->icon->setVisibility(aurore::Visibility::GONE);
    this->cover->addView(this->icon);
    this->addView(this->cover);

    this->name = new aurore::Label();
    this->name->setFontSize(17);
    this->name->setSingleLine(true);
    this->name->setMarginTop(8);
    this->addView(this->name);

    this->detail = new aurore::Label();
    this->detail->setFontSize(15);
    this->detail->setSingleLine(true);
    this->detail->setThemeColor("textColor", "cadran/text3");
    this->detail->setMarginTop(2);
    this->addView(this->detail);
}

void GameTile::set_name(const std::string& text)
{
    this->name->setText(text);
}

void GameTile::set_detail(const std::string& text)
{
    this->detail->setText(text);
}

void GameTile::set_detail_color(const char* key)
{
    this->detail->setThemeColor("textColor", key);
}

void GameTile::set_badge(const char* icon_name, const char* ink)
{
    if (this->badge) {
        this->cover->removeView(this->badge, true);
        this->badge = nullptr;
    }
    if (!icon_name) return;
    constexpr float SIDE = 28;
    this->badge = new aurore::Box(aurore::Axis::ROW);
    this->badge->setPositionType(aurore::PositionType::ABSOLUTE);
    this->badge->setPositionTop(6);
    this->badge->setPositionRight(6);
    this->badge->setWidth(SIDE);
    this->badge->setHeight(SIDE);
    this->badge->setCornerRadius(SIDE / 2);
    this->badge->setThemeColor("backgroundColor", ink);
    this->badge->setThemeColor("borderColor", "aurore/background");
    this->badge->setBorderThickness(2);
    this->badge->setAlignItems(aurore::AlignItems::CENTER);
    this->badge->setJustifyContent(aurore::JustifyContent::CENTER);
    this->badge->setShadowType(aurore::ShadowType::GENERIC);
    this->badge->setShadowVisibility(true);
    this->badge->addView(cadran::kit::icon(icon_name, 15, "aurore/background"));
    this->cover->addView(this->badge);
}

void GameTile::set_icon(const uint8_t* rgba, int width, int height)
{
    this->icon->setImageFromRGBA(rgba, width, height);
    if (!this->icon->getTexture()) return;
    this->placeholder->setVisibility(aurore::Visibility::GONE);
    this->icon->setVisibility(aurore::Visibility::VISIBLE);
    // Arriving in place of the gamepad: the cover fades in.
    if (!this->iconed && effects::motion()) {
        this->icon_since = motion::now();
        this->icon->setAlpha(0.02f);
    }
    this->iconed = true;
}

void GameTile::draw(NVGcontext* vg, float x, float y, float width, float height, aurore::Style style,
                    aurore::FrameContext* ctx)
{
    if (this->icon_since) {
        const float t = motion::progress(this->icon_since, FADE_US);
        this->icon->setAlpha(std::max(0.02f, motion::ease_out(t)));
        if (t >= 1.0f) this->icon_since = 0;
    }
    aurore::Box::draw(vg, x, y, width, height, style, ctx);
}
