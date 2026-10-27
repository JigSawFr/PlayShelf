// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/cover_placeholder.hpp"

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/core/theme_image.hpp>

void CoverPlaceholder::declare()
{
    brls::Theme::addImageSlot(SLOT, "", SLOT_SIDE);
}

CoverPlaceholder::CoverPlaceholder(float side, float glyph, float radius) : corner(radius)
{
    this->setWidth(side);
    this->setHeight(side);
    this->setShrink(0);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->addView(cadran::kit::icon("gamepad", glyph, "cadran/text3"));
}

void CoverPlaceholder::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                            brls::FrameContext* ctx)
{
    // Asked at each frame (a lookup): the theme's picture of the moment,
    // drawn once loaded (ThemeImages draws the screen again then).
    const std::shared_ptr<brls::ThemeImage> art = brls::Theme::image(SLOT);
    if (art && art->draw(vg, x, y, width, height, brls::BackgroundFit::COVER, nullptr, this->getAlpha(), this->corner))
        return;
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}
