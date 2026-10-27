// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/notice_bell.hpp"

#include <string>

#include <cmath>

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/effects.hpp>
#include <borealis/cadran/ui/motion.hpp>
#include <borealis/cadran/view/header_settings.hpp>
#include <borealis/cadran/view/version_badge.hpp>

#include "action/notices.hpp"
#include "ui/app_hooks.hpp"

static constexpr float SIZE = 40;
static constexpr const char* BELL_ID = "notice_bell";
// The swing of a new entry (µs), and the pop of a changed number.
static constexpr float SWING_US = 700000.0f;
static constexpr float POP_US   = 380000.0f;

NoticeBell::NoticeBell()
{
    this->setId(BELL_ID);
    this->setFocusable(false);
    this->setWidth(SIZE);
    this->setHeight(SIZE);
    this->setAlignSelf(brls::AlignSelf::CENTER);
    // A tap opens the list, as Y does (hitTest finds it, focusable or not).
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this, []() {
        if (ui::app_hooks::get().open_notices) ui::app_hooks::get().open_notices();
    }));
    this->listener = notices::listen([this]() { this->update(); });
    this->update();
}

NoticeBell::~NoticeBell()
{
    notices::unlisten(this->listener);
}

brls::View* NoticeBell::hitTest(brls::Point point)
{
    // brls::View::hitTest skips a view that cannot take the focus.
    if (this->getAlpha() == 0.0f || this->getVisibility() != brls::Visibility::VISIBLE) return nullptr;
    return this->getFrame().pointInside(point) ? this : nullptr;
}

void NoticeBell::update()
{
    const int to_do = notices::action_count();
    const int said  = notices::info_count();
    // Something new to do rings the bell once; a number that changed pops
    // (not the first reading: the bell is drawn as it is).
    if (this->counted && effects::motion()) {
        if (to_do > this->actions) this->swing_since = motion::now();
        if (to_do != this->actions || (to_do == 0 && (said > 0) != (this->infos > 0))) this->pop_since = motion::now();
    }
    this->actions = to_do;
    this->infos   = said;
    this->counted = true;
}

void NoticeBell::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                      brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;
    // The bell: a dome over a flared rim, and the clapper under it, in the
    // header's text colour (the notes' grey while nothing is there).
    const NVGcolor ink = this->actions || this->infos ? cadran::kit::color("cadran/text") : cadran::kit::color("cadran/text3");
    const float cx = x + width * 0.42f, top = y + height * 0.14f;
    const float w = width * 0.62f, h = height * 0.6f;
    // Swinging: the bell turns about its crown, less and less.
    nvgSave(vg);
    if (this->swing_since) {
        const float t = motion::progress(this->swing_since, SWING_US);
        if (t >= 1.0f) {
            this->swing_since = 0;
        } else {
            const float angle = 0.21f * std::sin(t * 3.0f * NVG_PI) * (1.0f - t);   // 12° at most
            nvgTranslate(vg, cx, top);
            nvgRotate(vg, angle);
            nvgTranslate(vg, -cx, -top);
        }
    }
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx - w * 0.5f, top + h);
    nvgBezierTo(vg, cx - w * 0.38f, top + h * 0.78f, cx - w * 0.38f, top + h * 0.55f, cx - w * 0.36f, top + h * 0.42f);
    nvgBezierTo(vg, cx - w * 0.32f, top + h * 0.08f, cx - w * 0.12f, top, cx, top);
    nvgBezierTo(vg, cx + w * 0.12f, top, cx + w * 0.32f, top + h * 0.08f, cx + w * 0.36f, top + h * 0.42f);
    nvgBezierTo(vg, cx + w * 0.38f, top + h * 0.55f, cx + w * 0.38f, top + h * 0.78f, cx + w * 0.5f, top + h);
    nvgClosePath(vg);
    nvgFillColor(vg, ink);
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx, top + h + height * 0.08f, height * 0.07f);
    nvgFill(vg);
    nvgRestore(vg);

    // What it holds: the number to do on an amber disc, else a dot for
    // information alone. A changed number pops in.
    const float bx = x + width * 0.76f, by = y + height * 0.24f;
    nvgSave(vg);
    if (this->pop_since) {
        const float t = motion::progress(this->pop_since, POP_US);
        if (t >= 1.0f) {
            this->pop_since = 0;
        } else {
            const float scale = motion::back_out(t);
            nvgTranslate(vg, bx, by);
            nvgScale(vg, scale, scale);
            nvgTranslate(vg, -bx, -by);
        }
    }
    if (this->actions > 0) {
        const std::string n = this->actions > 9 ? "9+" : std::to_string(this->actions);
        const float r = height * 0.24f;
        nvgBeginPath(vg);
        nvgCircle(vg, bx, by, r);
        nvgFillColor(vg, cadran::kit::color("cadran/amber"));
        nvgFill(vg);
        nvgFontFaceId(vg, brls::Application::getDefaultFont());
        nvgFontSize(vg, r * 1.45f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, brls::Application::getTheme()["brls/background"]);
        nvgText(vg, bx, by + r * 0.06f, n.c_str(), nullptr);
    } else if (this->infos > 0) {
        nvgBeginPath(vg);
        nvgCircle(vg, bx, by, height * 0.12f);
        nvgFillColor(vg, cadran::kit::color("cadran/text3"));
        nvgFill(vg);
    }
    nvgRestore(vg);
}

// The main screen (its tab frame) holding `focus`: the bell goes in the
// header's row at the right, after the language, theme and text size
// (Aurore's, which may be there first), unless it has one.
static void bell_screen_of(brls::View* focus)
{
    if (!focus) return;
    while (focus->hasParent()) focus = focus->getParent();
    auto* frame = dynamic_cast<brls::AppletFrame*>(focus);
    if (!frame || !frame->getView("main_tabs")) return;
    auto* tools = VersionBadge::header_tools(frame);
    if (!tools || tools->getView(BELL_ID)) return;
    size_t at = 0;
    for (size_t i = 0; i < tools->getChildren().size(); i++)
        if (dynamic_cast<HeaderSetting*>(tools->getChildren()[i])) at = i + 1;
    tools->addView(new NoticeBell(), at);
}

void NoticeBell::install()
{
    brls::Application::getGlobalFocusChangeEvent()->subscribe(bell_screen_of);
}
