// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/tool_chip.hpp"

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/view/cadran_icon.hpp>

namespace
{
constexpr float HEIGHT = 42;
}   // namespace

ToolChip::ToolChip(const char* icon_name, const std::string& words)
{
    this->setAxis(aurore::Axis::ROW);
    this->setAlignItems(aurore::AlignItems::CENTER);
    this->setHeight(HEIGHT);
    this->setPadding(0, 18, 0, 14);
    this->setMargins(0, 10, 10, 0);
    this->setCornerRadius(HEIGHT / 2);
    this->setHighlightCornerRadius(HEIGHT / 2 + 2);
    this->setThemeColor("backgroundColor", "cadran/surf");
    this->setThemeColor("borderColor", "cadran/line");
    this->setBorderThickness(1);
    this->setShrink(0);
    this->setFocusable(true);
    this->icon = cadran::kit::icon(icon_name, 18, "cadran/text2");
    this->icon->setMarginRight(9);
    this->addView(this->icon);
    this->what = cadran::kit::label(words, 15, "cadran/text3");
    this->what->setSingleLine(true);
    this->what->setMarginRight(8);
    this->what->setVisibility(words.empty() ? aurore::Visibility::GONE : aurore::Visibility::VISIBLE);
    this->addView(this->what);
    this->value = cadran::kit::label(" ", 16, "cadran/text");
    this->value->setSingleLine(true);
    this->value->setMaxWidth(320);
    this->addView(this->value);
}

void ToolChip::set_value(const std::string& text)
{
    if (this->value->getFullText() != text) this->value->setText(text);
}

void ToolChip::set_active(bool on)
{
    if (on == this->active) return;
    this->active = on;
    this->setThemeColor("backgroundColor", on ? "cadran/teal_soft" : "cadran/surf");
    this->setThemeColor("borderColor", on ? "cadran/teal" : "cadran/line");
    this->icon->setThemeColor("color", on ? "cadran/teal" : "cadran/text2");
    this->value->setThemeColor("textColor", on ? "cadran/teal" : "cadran/text");
}
