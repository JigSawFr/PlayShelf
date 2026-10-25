// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/footer_hints.hpp"

#include "ui/cadran_kit.hpp"
#include "ui/effects.hpp"
#include "ui/guarded.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>
#include <string>

// The timed hint (set_timed): one at a time, the footer's own.
static std::string s_timed_text;
static brls::Time  s_timed_until = 0;
static brls::Time  s_timed_total = 0;

// Each hint's room on either side and between its button and its text
// (borealis' hint XML), and the least they get here: 2 × 6 px still part
// one hint from the next.
static constexpr float HINT_PADDING     = 16;
static constexpr float HINT_PADDING_MIN = 6;
static constexpr float HINT_GAP         = 8;
static constexpr float HINT_GAP_MIN     = 4;
// Kept between the clock and the first hint.
static constexpr float CLOCK_GAP = 16;

FooterHints::FooterHints()
{
    // No greyed "A OK" where A does nothing: the footer says only what the
    // keys do.
    this->setAddUnableAButtonAction(false);
    // After brls::Hints' own subscription (its constructor's): the hints are
    // refilled when this runs. Their layout is computed at once (View::
    // invalidate), so fit() reads their width before the next frame.
    this->subscription = brls::Application::getGlobalHintsUpdateEvent()->subscribe([this]() { this->fit(); });
}

FooterHints::~FooterHints()
{
    brls::Application::getGlobalHintsUpdateEvent()->unsubscribe(this->subscription);
}

void FooterHints::fit()
{
    // Only in the footer (brls::BottomBar), next to its clock; the keyboard
    // dialog's hints have none.
    brls::Box* row = this->getParent();
    if (!row || this->getChildren().empty())
        return;
    // A footer that is not shown (a dialog's, footerHidden) is never drawn:
    // its clock stays empty, and the wait below would come back every frame
    // for as long as the dialog is open.
    for (brls::View* v = this; v; v = v->getParent())
        if (v->getVisibility() != brls::Visibility::VISIBLE) return;
    brls::View* status = nullptr;   // battery, Wi-Fi and clock
    for (brls::View* v : row->getChildren())
        if (v != this) status = v;
    auto* clock = status ? dynamic_cast<brls::Label*>(status->getView("brls/hints/time")) : nullptr;
    if (!clock)
        return;
    // A new screen's clock is set when it is first drawn: until then it has
    // no width of its own. Fitted after that frame.
    if (clock->getFullText().empty()) {
        brls::sync(ui::guarded(this->alive, [this]() { this->fit(); }));
        return;
    }

    clock->setSingleLine(true);
    status->setShrink(0);
    this->setShrink(0);   // the hints at the width they ask for

    // The row has no padding on its sides (borealis' bottom_bar.cpp).
    const float room = row->getWidth() - status->getWidth() - CLOCK_GAP;
    const float over = this->getWidth() - room;
    if (over <= 0)
        return;

    // Closer together, by no more than they need: less room on their sides
    // first, then between button and text. Refilled hints are new views, so
    // each starts from borealis' spacing again.
    float need = std::ceil(over / this->getChildren().size());   // per hint
    const float padding_cut = std::min(std::ceil(need / 2), HINT_PADDING - HINT_PADDING_MIN);
    need = std::max(0.0f, need - 2 * padding_cut);
    const float gap_cut = std::min(need, HINT_GAP - HINT_GAP_MIN);
    for (brls::View* v : this->getChildren()) {
        auto* hint = dynamic_cast<brls::Box*>(v);
        if (!hint) continue;
        hint->setPaddingLeft(HINT_PADDING - padding_cut);
        hint->setPaddingRight(HINT_PADDING - padding_cut);
        if (brls::View* text = hint->getView("hint"))
            text->setMarginLeft(HINT_GAP - gap_cut);
        // The button's glyph whole: only the text is cut below.
        if (brls::View* icon = hint->getView("icon")) icon->setShrink(0);
    }
    // Still too wide: the texts are cut ("…"), the clock is not.
    if (this->getWidth() > room)
        this->setShrink(1);
}

void FooterHints::set_timed(const std::string& hint_text, brls::Time until, brls::Time total_us)
{
    s_timed_text  = hint_text;
    s_timed_until = until;
    s_timed_total = total_us;
}

void FooterHints::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                       brls::FrameContext* ctx)
{
    brls::Hints::draw(vg, x, y, width, height, style, ctx);
    if (s_timed_text.empty() || !s_timed_until || !effects::motion()) return;
    const brls::Time now = motion::now();
    if (now >= s_timed_until || s_timed_total <= 0) return;
    const float left = (float)(s_timed_until - now) / (float)s_timed_total;   // 1 → 0
    constexpr float LAST_SECOND_US = 1000000.0f;
    for (brls::View* v : this->getChildren()) {
        auto* hint = dynamic_cast<brls::Box*>(v);
        auto* text = hint ? dynamic_cast<brls::Label*>(hint->getView("hint")) : nullptr;
        if (!text || text->getFullText() != s_timed_text) continue;
        // Fading over its last second: the eye is told it is about to go.
        const float fade = std::min(1.0f, (float)(s_timed_until - now) / LAST_SECOND_US);
        hint->setAlpha(std::max(0.02f, fade));
        // The line, under the hint's words, as long as the time left.
        const float lx = text->getX(), lw = text->getWidth() * left;
        const float ly = hint->getY() + hint->getHeight() - 3.0f;
        NVGcolor teal = cadran::kit::color("cadran/teal_fill");
        teal.a *= fade;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, lx, ly, std::max(2.0f, lw), 2.0f, 1.0f);
        nvgFillColor(vg, teal);
        nvgFill(vg);
        return;
    }
}

void FooterHints::install()
{
    brls::Application::registerXMLView("brls:Hints", FooterHints::create);
}

brls::View* FooterHints::create()
{
    return new FooterHints();
}
