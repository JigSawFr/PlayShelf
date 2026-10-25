// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/header_settings.hpp"

#include <cctype>
#include <cmath>
#include <string>

#include "ui/cadran_list.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/live_prefs.hpp"
#include "ui/text_size.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "view/version_badge.hpp"

static constexpr float SIZE = 40;
static constexpr const char* LANGUAGE_ID = "header_language";
static constexpr const char* THEME_ID    = "header_theme";
static constexpr const char* TEXT_ID     = "header_text_size";

// "FR" for "fr" and "fr-CA", "ZH" for "zh-Hans": the language, not the region
// (the list says which one). "system": the language the console gave borealis.
static std::string language_code()
{
    std::string locale = config::get().language;
    if (locale == "system") locale = live_prefs::locale();
    std::string code;
    for (char c : locale) {
        if (c == '-' || c == '_') break;
        code += (char)std::toupper((unsigned char)c);
    }
    return code.empty() ? "?" : code;
}

HeaderSetting::HeaderSetting(Kind setting)
    : kind(setting)
{
    this->setId(setting == Kind::LANGUAGE ? LANGUAGE_ID : setting == Kind::THEME ? THEME_ID : TEXT_ID);
    this->setFocusable(false);
    this->setWidth(setting == Kind::LANGUAGE ? SIZE * 1.3f : SIZE);
    this->setHeight(SIZE);
    this->setAlignSelf(brls::AlignSelf::CENTER);
    this->setMarginRight(10);
    // A tap opens its list (hitTest finds it, focusable or not); the tab on
    // screen shows the new choice (Preferences says it too).
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this, [setting]() {
        if (setting == Kind::LANGUAGE) ui::pick_language(ui::refresh_shown_tab);
        else if (setting == Kind::THEME) ui::pick_theme(ui::refresh_shown_tab);
        else ui::pick_text_size(ui::refresh_shown_tab);
    }));
}

brls::View* HeaderSetting::hitTest(brls::Point point)
{
    // brls::View::hitTest skips a view that cannot take the focus.
    if (this->getAlpha() == 0.0f || this->getVisibility() != brls::Visibility::VISIBLE) return nullptr;
    return this->getFrame().pointInside(point) ? this : nullptr;
}

void HeaderSetting::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                         brls::FrameContext* ctx)
{
    (void)style;
    (void)ctx;
    if (this->kind == Kind::LANGUAGE) this->draw_language(vg, x, y, width, height);
    else if (this->kind == Kind::THEME) this->draw_theme(vg, x, y, width, height);
    else this->draw_text_size(vg, x, y, width, height);
}

void HeaderSetting::draw_language(NVGcontext* vg, float x, float y, float width, float height)
{
    // The code in the header's text colour, in a thin rounded frame.
    const NVGcolor ink = cadran::kit::color("cadran/text");
    const float inset = height * 0.16f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 1, y + inset, width - 2, height - 2 * inset, 6);
    nvgStrokeColor(vg, ink);
    nvgStrokeWidth(vg, 2);
    nvgStroke(vg);
    nvgFontFaceId(vg, brls::Application::getDefaultFont());
    nvgFontSize(vg, height * 0.45f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, ink);
    const std::string code = language_code();
    nvgText(vg, x + width / 2, y + height / 2 + height * 0.02f, code.c_str(), nullptr);
}

void HeaderSetting::draw_theme(NVGcontext* vg, float x, float y, float width, float height)
{
    const NVGcolor ink = cadran::kit::color("cadran/text");
    const float cx = x + width / 2, cy = y + height / 2;
    const std::string& theme = config::get().theme;
    if (theme == "light") {
        // A sun: a disc and eight rays.
        const float r = height * 0.18f;
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgFillColor(vg, ink);
        nvgFill(vg);
        nvgBeginPath(vg);
        for (int i = 0; i < 8; i++) {
            const float a = (float)i * NVG_PI / 4;
            nvgMoveTo(vg, cx + std::cos(a) * r * 1.55f, cy + std::sin(a) * r * 1.55f);
            nvgLineTo(vg, cx + std::cos(a) * r * 2.25f, cy + std::sin(a) * r * 2.25f);
        }
        nvgStrokeColor(vg, ink);
        nvgStrokeWidth(vg, 2.5f);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);
    } else if (theme == "dark") {
        // A crescent moon: the disc's far side, then back along the edge of
        // the disc that bites it (offset up and right), between the two
        // points where they cross. (A hole wider than the disc leaves a ring.)
        const float r = height * 0.32f, R = r * 0.85f;
        const float ox = r * 0.55f, oy = -r * 0.35f, d = std::sqrt(ox * ox + oy * oy);
        const float t = std::atan2(oy, ox);
        const float along = (d * d + r * r - R * R) / (2 * d);   // the crossing points, along the offset
        const float a = std::acos(along / r);                     // their angle on the disc
        const float b = std::acos((along - d) / R);               // and on the bite
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, r, t + a, t + 2 * NVG_PI - a, NVG_CW);
        nvgArc(vg, cx + ox, cy + oy, R, t - b, t + b - 2 * NVG_PI, NVG_CCW);
        nvgClosePath(vg);
        nvgFillColor(vg, ink);
        nvgFill(vg);
    } else {
        // The console's: a circle, its left half filled (light or dark, as the
        // console says).
        const float r = height * 0.3f;
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, r, NVG_PI / 2, NVG_PI * 3 / 2, NVG_CW);
        nvgClosePath(vg);
        nvgFillColor(vg, ink);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgStrokeColor(vg, ink);
        nvgStrokeWidth(vg, 2.5f);
        nvgStroke(vg);
    }
}

void HeaderSetting::draw_text_size(NVGcontext* vg, float x, float y, float width, float height)
{
    // "AA": a small A, then a large one on the same baseline, teal while
    // the text is larger.
    const NVGcolor ink = cadran::kit::color("cadran/text");
    const float base = y + height * 0.78f;
    nvgFontFaceId(vg, brls::Application::getDefaultFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    nvgFontSize(vg, height * 0.5f);
    float bounds[4];
    const float small = nvgTextBounds(vg, 0, 0, "A", nullptr, bounds);
    nvgFontSize(vg, height * 0.78f);
    const float large = nvgTextBounds(vg, 0, 0, "A", nullptr, bounds);
    const float gap = height * 0.04f;
    const float left = x + (width - small - gap - large) / 2;
    nvgFontSize(vg, height * 0.5f);
    nvgFillColor(vg, ink);
    nvgText(vg, left, base, "A", nullptr);
    nvgFontSize(vg, height * 0.78f);
    nvgFillColor(vg, text_size::larger() ? brls::Application::getTheme()["cadran/teal"] : ink);
    nvgText(vg, left + small + gap, base, "A", nullptr);
}

// The main screen (its tab frame) holding `focus`: the language then the
// theme go first in the header's row at the right (the bell is there
// already), unless it has them.
static void settings_screen_of(brls::View* focus)
{
    if (!focus) return;
    while (focus->hasParent()) focus = focus->getParent();
    auto* frame = dynamic_cast<brls::AppletFrame*>(focus);
    if (!frame || !frame->getView("main_tabs")) return;
    auto* tools = VersionBadge::header_tools(frame);
    if (!tools || tools->getView(LANGUAGE_ID)) return;
    tools->addView(new HeaderSetting(HeaderSetting::Kind::LANGUAGE), 0);
    tools->addView(new HeaderSetting(HeaderSetting::Kind::THEME), 1);
    tools->addView(new HeaderSetting(HeaderSetting::Kind::TEXT_SIZE), 2);
}

void HeaderSetting::install()
{
    brls::Application::getGlobalFocusChangeEvent()->subscribe(settings_screen_of);
}
