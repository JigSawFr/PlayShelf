// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/cover_placeholder.hpp"

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/core/theme_image.hpp>

void CoverPlaceholder::declare()
{
    aurore::Theme::addImageSlot(SLOT, "", SLOT_SIDE);
}

CoverPlaceholder::CoverPlaceholder(float side, float glyph, float radius) : corner(radius)
{
    this->setWidth(side);
    this->setHeight(side);
    this->setShrink(0);
    this->setAlignItems(aurore::AlignItems::CENTER);
    this->setJustifyContent(aurore::JustifyContent::CENTER);
    this->addView(cadran::kit::icon("gamepad", glyph, "cadran/text3"));
}

void CoverPlaceholder::draw(NVGcontext* vg, float x, float y, float width, float height, aurore::Style style,
                            aurore::FrameContext* ctx)
{
    // Asked at each frame (a lookup): the theme's picture of the moment,
    // drawn once loaded (ThemeImages draws the screen again then).
    const std::shared_ptr<aurore::ThemeImage> art = aurore::Theme::image(SLOT);
    if (art && art->draw(vg, x, y, width, height, aurore::BackgroundFit::COVER, nullptr, this->getAlpha(), this->corner))
        return;
    aurore::Box::draw(vg, x, y, width, height, style, ctx);
}
