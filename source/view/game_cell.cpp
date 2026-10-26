// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/game_cell.hpp"

#include <borealis/cadran/ui/cadran_kit.hpp>

namespace
{
constexpr float ICON = 44;
}   // namespace

GameCell::GameCell()
{
    this->slot = new brls::Box(brls::Axis::ROW);
    this->slot->setWidth(ICON);
    this->slot->setHeight(ICON);
    this->slot->setMarginRight(16);
    this->slot->setShrink(0);
    this->slot->setCornerRadius(8);
    this->slot->setBackgroundColor(cadran::kit::color("cadran/well"));
    this->slot->setAlignItems(brls::AlignItems::CENTER);
    this->slot->setJustifyContent(brls::JustifyContent::CENTER);
    this->placeholder = cadran::kit::icon("gamepad", 22, "cadran/text3");
    this->slot->addView(this->placeholder);
    this->icon = new brls::Image();
    this->icon->setWidth(ICON);
    this->icon->setHeight(ICON);
    this->icon->setCornerRadius(8);
    this->icon->setScalingType(brls::ImageScalingType::FIT);
    this->icon->setVisibility(brls::Visibility::GONE);
    this->slot->addView(this->icon);
    this->addView(this->slot, 0);   // before the title
}

void GameCell::set_icon_texture(int texture, bool owned)
{
    if (texture <= 0) return;
    this->icon->innerSetImage(texture);
    this->icon->setFreeTexture(owned);
    this->icon->setVisibility(brls::Visibility::VISIBLE);
    this->placeholder->setVisibility(brls::Visibility::GONE);
}
