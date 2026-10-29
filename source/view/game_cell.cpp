// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/game_cell.hpp"

#include "view/cover_placeholder.hpp"

namespace
{
constexpr float ICON = 44;
}   // namespace

GameCell::GameCell()
{
    this->slot = new aurore::Box(aurore::Axis::ROW);
    this->slot->setWidth(ICON);
    this->slot->setHeight(ICON);
    this->slot->setMarginRight(16);
    this->slot->setShrink(0);
    this->slot->setCornerRadius(8);
    this->slot->setThemeColor("backgroundColor", "cadran/well");
    this->slot->setAlignItems(aurore::AlignItems::CENTER);
    this->slot->setJustifyContent(aurore::JustifyContent::CENTER);
    this->placeholder = new CoverPlaceholder(ICON, 22, 8);
    this->slot->addView(this->placeholder);
    this->icon = new aurore::Image();
    this->icon->setWidth(ICON);
    this->icon->setHeight(ICON);
    this->icon->setCornerRadius(8);
    this->icon->setScalingType(aurore::ImageScalingType::FIT);
    this->icon->setVisibility(aurore::Visibility::GONE);
    this->slot->addView(this->icon);
    this->addView(this->slot, 0);   // before the title
}

void GameCell::set_icon(const uint8_t* rgba, int width, int height)
{
    this->icon->setImageFromRGBA(rgba, width, height);
    if (!this->icon->getTexture()) return;
    this->icon->setVisibility(aurore::Visibility::VISIBLE);
    this->placeholder->setVisibility(aurore::Visibility::GONE);
}
