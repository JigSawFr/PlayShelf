// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/funding.hpp"

#include "app.hpp"
#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/util/text.hpp>

#include "ui/ui.hpp"

using namespace aurore::literals;

namespace funding
{
namespace
{
void open_large(const std::string& name, const std::string& url)
{
    aurore::Box* code = cadran::kit::qr(url, 340, name, text::short_url(url));
    code->setAlignSelf(aurore::AlignSelf::CENTER);
    auto* dialog = ui::dialog("", code);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

aurore::Box* card(const std::string& name, const std::string& url, float qr_side, bool focusable)
{
    // Both cards alike, whatever their address.
    aurore::Box* card = cadran::kit::qr(url, qr_side, name, text::short_url(url), qr_side + 160);
    card->setPadding(12, 16, 12, 16);
    if (focusable) {
        card->setFocusable(true);
        card->setHideHighlightBackground(true);
        card->setHighlightCornerRadius(cadran::kit::RADIUS_CARD);
        card->registerClickAction([name, url](aurore::View*) {
            open_large(name, url);
            return true;
        });
    }
    return card;
}
}   // namespace

aurore::Box* cards(float qr_side, bool focusable)
{
    auto* row = new aurore::Box(aurore::Axis::ROW);
    row->setJustifyContent(aurore::JustifyContent::SPACE_EVENLY);
    row->addView(card("GitHub Sponsors", app::SPONSORS_URL, qr_side, focusable));
    row->addView(card("Ko-fi", app::KOFI_URL, qr_side, focusable));
    return row;
}

aurore::Dialog* dialog(const std::string& text, float qr_side)
{
    return ui::dialog(text, cards(qr_side, false));
}

void open_dialog(std::function<void()> then)
{
    auto* d = dialog("playshelf/about/support_body"_i18n, 180);
    d->addButton("hints/ok"_i18n, [then]() {
        if (then) then();
    });
    d->open();
}

}   // namespace funding
