// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/cadran_kit.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fmt/format.h>
#include <tuple>
#include <yoga/Yoga.h>

#include "ui/cadran_list.hpp"
#include "ui/effects.hpp"
#include "ui/motion.hpp"
#include "ui/ui.hpp"
#include "view/cadran_digits.hpp"
#include "view/qr_view.hpp"

namespace cadran::kit
{

NVGcolor color(const char* key)
{
    return brls::Application::getTheme()[key];
}

brls::Label* label(const std::string& text, float size, const char* color_key)
{
    auto* l = new brls::Label();
    l->setText(text);
    l->setFontSize(size);
    l->setTextColor(brls::Application::getTheme()[color_key]);
    return l;
}

namespace
{
// A label in the display face: Sora, the other scripts from the app's fonts.
class DisplayLabel : public brls::Label
{
  public:
    DisplayLabel()
    {
        const int face = brls::Application::getFont("cadran/digits");
        if (face != brls::FONT_INVALID) this->font = face;
    }
};
}   // namespace

brls::Label* display(const std::string& text, float size, const char* color_key)
{
    auto* l = new DisplayLabel();
    l->setText(text);
    l->setFontSize(size);
    l->setTextColor(brls::Application::getTheme()[color_key]);
    return l;
}

CadranIcon* icon(const char* name, float size, const char* color_key)
{
    auto* view = new CadranIcon();
    view->setIcon(name);
    view->setWidth(size);
    view->setHeight(size);
    view->setColor(brls::Application::getTheme()[color_key]);
    return view;
}

brls::Box* card(bool focusable, bool grow)
{
    brls::Theme theme = brls::Application::getTheme();
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::STRETCH);
    box->setBackgroundColor(theme["cadran/surf"]);
    box->setBorderColor(theme["cadran/line"]);
    box->setBorderThickness(1);
    box->setCornerRadius(RADIUS_CARD);
    box->setPadding(20, 24, 20, 24);
    if (focusable) {
        box->setFocusable(true);
        box->setHideHighlightBackground(true);
        box->setHighlightCornerRadius(RADIUS_CARD);
    }
    if (grow) {
        box->setGrow(1.0f);
        box->setWidth(0);
    }
    return box;
}

brls::Box* top_card(bool grow)
{
    brls::Box* box = card(false, grow);
    box->setCornerRadius(RADIUS_TOP);
    box->setHighlightCornerRadius(RADIUS_TOP);
    box->setPadding(22, 26, 22, 26);
    return box;
}

void set_kicker(brls::Label* label, const std::string& text)
{
    if (!label) return;
    label->setFontSize(TEXT_KICKER);
    label->setTextColor(color("cadran/text2"));
    if (!text.empty()) label->setText(cadran::upper(text));
}

brls::Box* kicker(const char* icon_name, const std::string& title)
{
    auto* head = new brls::Box(brls::Axis::ROW);
    head->setAlignItems(brls::AlignItems::CENTER);
    head->addView(icon(icon_name, 18, "cadran/text2"));
    auto* text = new brls::Label();
    set_kicker(text, title);
    text->setSingleLine(true);
    text->setMarginLeft(8);
    head->addView(text);
    return head;
}

Mark::Mark(const char* color_key)
{
    this->setFocusable(false);
    this->setWidth(12);
    this->setHeight(12);
    this->set(color_key);
}

void Mark::set(const char* color_key)
{
    const std::string key = color_key;
    this->color = brls::Application::getTheme()[color_key];
    this->shape = key.rfind("cadran/teal", 0) == 0                                          ? Shape::CHECK
                  : key.rfind("cadran/amber", 0) == 0 || key.rfind("cadran/coral", 0) == 0 ? Shape::WARN
                                                                                           : Shape::DOT;
}

void Mark::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*)
{
    const float cx = x + width / 2, cy = y + height / 2;
    nvgBeginPath(vg);
    switch (this->shape) {
        case Shape::DOT:
            nvgCircle(vg, cx, cy, 4);
            nvgFillColor(vg, this->a(this->color));
            nvgFill(vg);
            break;
        case Shape::CHECK:
            nvgMoveTo(vg, x + 1.5f, cy + 0.5f);
            nvgLineTo(vg, x + 4.8f, cy + 3.6f);
            nvgLineTo(vg, x + width - 1.2f, y + 2.6f);
            nvgStrokeColor(vg, this->a(this->color));
            nvgStrokeWidth(vg, 2.2f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);
            break;
        case Shape::WARN: {
            // A rounded triangle, an exclamation mark cut out of it.
            nvgMoveTo(vg, cx, y + 0.5f);
            nvgLineTo(vg, x + width, y + height - 0.5f);
            nvgLineTo(vg, x, y + height - 0.5f);
            nvgClosePath(vg);
            nvgFillColor(vg, this->a(this->color));
            nvgFill(vg);
            const NVGcolor cut = this->a(brls::Application::getTheme()["brls/background"]);
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx - 0.8f, y + 4.2f, 1.6f, 3.8f, 0.8f);
            nvgCircle(vg, cx, y + height - 2.6f, 0.95f);
            nvgFillColor(vg, cut);
            nvgFill(vg);
            break;
        }
    }
}

brls::Box* pill(const std::string& text, const char* dot_key)
{
    brls::Theme theme = brls::Application::getTheme();
    auto* box = new brls::Box(brls::Axis::ROW);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setHeight(30);
    box->setPaddingLeft(14);
    box->setPaddingRight(16);
    box->setCornerRadius(15);
    box->setBorderColor(theme["cadran/line"]);
    box->setBorderThickness(1);
    box->setMarginRight(10);
    box->setMarginBottom(8);
    box->setHighlightCornerRadius(15);
    auto* mark = new Mark(dot_key);
    mark->setMarginRight(7);
    box->addView(mark);
    brls::Label* l = label(text, TEXT_META, "cadran/text2");
    l->setSingleLine(true);
    box->addView(l);
    return box;
}

brls::Box* key(const char* letter, float size)
{
    brls::Theme theme = brls::Application::getTheme();
    auto* bubble = new brls::Box(brls::Axis::ROW);
    bubble->setWidth(size);
    bubble->setHeight(size);
    bubble->setCornerRadius(size / 2);
    bubble->setShrink(0);
    bubble->setBackgroundColor(theme["cadran/key_bg"]);
    bubble->setAlignItems(brls::AlignItems::CENTER);
    bubble->setJustifyContent(brls::JustifyContent::CENTER);
    auto* text = new CadranDigits();
    text->setFontSize(12);
    text->setTextColor(theme["cadran/key_fg"]);
    text->setText(letter);
    bubble->addView(text);
    return bubble;
}

Chip::Chip()
    : brls::Box(brls::Axis::ROW)
{
}

void Chip::frame(brls::FrameContext* ctx)
{
    // The press's flash (borealis' click animation, 0 → 1 → 0) as a dip of
    // the whole chip, background and words, about its middle.
    const float press = effects::motion() ? this->getClickAlpha() : 0.0f;
    if (press <= 0.0f) {
        brls::Box::frame(ctx);
        return;
    }
    const brls::Rect at = this->getFrame();
    const float cx = at.getMidX(), cy = at.getMidY();
    const float scale = 1.0f - 0.05f * press;
    nvgSave(ctx->vg);
    nvgTranslate(ctx->vg, cx, cy);
    nvgScale(ctx->vg, scale, scale);
    nvgTranslate(ctx->vg, -cx, -cy);
    brls::Box::frame(ctx);
    nvgRestore(ctx->vg);
}

brls::Box* chip(const std::string& text, float height)
{
    brls::Theme theme = brls::Application::getTheme();
    auto* box = new Chip();
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setHeight(height);
    box->setPaddingLeft(22);
    box->setPaddingRight(22);
    box->setCornerRadius(height / 2);
    box->setBackgroundColor(theme["cadran/surf2"]);
    box->setBorderColor(theme["cadran/line"]);
    box->setBorderThickness(1);
    box->setFocusable(true);
    box->setHideHighlightBackground(true);
    box->setHighlightCornerRadius(height / 2);
    auto* digits = new CadranDigits();
    digits->setText(text);
    digits->setFontSize(17);
    digits->setFollowsTextSize(true);
    box->addView(digits);
    return box;
}

brls::Box* qr(const std::string& payload, float side, const std::string& title, const std::string& caption, float width)
{
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    if (width > 0) box->setWidth(width);
    box->addView(new QrView(payload, side));
    for (const auto& [text, size, key] : { std::tuple<const std::string&, float, const char*>{ title, 18, "cadran/text" },
                                           { caption, TEXT_META, "cadran/text3" } }) {
        if (text.empty()) continue;
        brls::Label* l = label(text, size, key);
        l->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        l->setAlignSelf(brls::AlignSelf::STRETCH);   // wraps at the box's width
        l->setMarginTop(&text == &title ? 10 : 2);
        box->addView(l);
    }
    return box;
}

Bar::Bar()
{
    this->setHeight(12);
    this->setFocusable(false);
}

void Bar::set(float value)
{
    value = value < 0 ? 0 : value > 1 ? 1 : value;
    if (value == this->fraction) return;
    // From where the fill is drawn now, over a third of a second.
    this->from     = this->drawn();
    this->since    = effects::motion() ? motion::now() : 0;
    this->fraction = value;
}

float Bar::drawn() const
{
    const float t = motion::ease_out(motion::progress(this->since, 350000.0f));
    return this->from + (this->fraction - this->from) * t;
}

void Bar::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*)
{
    brls::Theme theme = brls::Application::getTheme();
    const float radius = height / 2.0f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, radius);
    nvgFillColor(vg, this->a(theme["cadran/well"]));
    nvgFill(vg);
    const float fill = this->drawn();
    if (fill <= 0) return;
    const float w = std::max(width * fill, height);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, height, radius);
    nvgFillColor(vg, this->a(theme["cadran/teal_fill"]));
    nvgFill(vg);
}

Empty empty(const char* icon_name)
{
    brls::Theme theme = brls::Application::getTheme();
    Empty e;
    e.box = card(false, false);
    e.box->setAlignItems(brls::AlignItems::CENTER);
    e.box->setPadding(32, 40, 32, 40);
    auto* circle = new brls::Box(brls::Axis::ROW);
    circle->setWidth(64);
    circle->setHeight(64);
    circle->setCornerRadius(32);
    circle->setBackgroundColor(theme["cadran/teal_soft"]);
    circle->setAlignItems(brls::AlignItems::CENTER);
    circle->setJustifyContent(brls::JustifyContent::CENTER);
    circle->addView(icon(icon_name, 28, "cadran/teal"));
    e.box->addView(circle);
    e.title = label("", TEXT_LEAD, "cadran/text");
    e.title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    e.title->setAlignSelf(brls::AlignSelf::STRETCH);   // wraps at the card's width
    e.title->setMarginTop(16);
    e.box->addView(e.title);
    e.text = label("", TEXT_META, "cadran/text2");
    e.text->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    e.text->setAlignSelf(brls::AlignSelf::STRETCH);
    e.text->setMarginTop(6);
    e.box->addView(e.text);
    return e;
}

brls::Box* empty_around(brls::Label* label, const char* icon_name)
{
    auto* parent = label ? dynamic_cast<brls::Box*>(label->getParent()) : nullptr;
    if (!parent) return nullptr;
    Empty e = empty(icon_name);
    e.box->setId("cadran_empty_card");
    const YGValue top = YGNodeStyleGetMargin(label->getYGNode(), YGEdgeTop);
    e.box->setMargins(top.unit == YGUnitPoint && top.value > 10 ? top.value : 10, 0, 16, 0);
    const auto& children = parent->getChildren();
    const size_t at = (size_t)(std::find(children.begin(), children.end(), label) - children.begin());
    // The label in the title's place, laid out as it.
    parent->removeView(label, false);
    e.box->removeView(e.title);
    label->setFontSize(TEXT_LEAD);
    label->setTextColor(color("cadran/text"));
    label->setSingleLine(false);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setAlignSelf(brls::AlignSelf::STRETCH);
    label->setMargins(16, 0, 0, 0);
    e.box->addView(label, 1);
    e.box->removeView(e.text);   // the screen's label says it all
    parent->addView(e.box, at);
    e.box->setVisibility(label->getVisibility());
    label->setVisibility(brls::Visibility::VISIBLE);
    return e.box;
}

void wrap(brls::View* row)
{
    if (row) YGNodeStyleSetFlexWrap(row->getYGNode(), YGWrapWrap);
}

void fit_button(brls::Button* button)
{
    auto* label = button ? dynamic_cast<brls::Label*>(button->getView("brls/button/label")) : nullptr;
    if (!label) return;
    label->setSingleLine(true);
    label->setShrinkToFit(true);
    // Its width at the normal size: the text's at the size asked, its
    // padding, and the views beside it (an icon), whose sizes are fixed.
    NVGcontext* vg = brls::Application::getNVGContext();
    nvgFontSize(vg, label->getFontSize());
    nvgFontFaceId(vg, label->getFont());
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    float bounds[4];
    nvgTextBounds(vg, 0, 0, label->getFullText().c_str(), nullptr, bounds);
    YGNodeRef node = button->getYGNode();
    const auto points = [](YGValue v) { return v.unit == YGUnitPoint ? v.value : 0.0f; };
    float width = bounds[2] - bounds[0] + points(YGNodeStyleGetPadding(node, YGEdgeLeft))
                  + points(YGNodeStyleGetPadding(node, YGEdgeRight)) + 1.0f;
    for (brls::View* child : button->getChildren()) {
        if (child == label || child->getVisibility() == brls::Visibility::GONE) continue;
        YGNodeRef c = child->getYGNode();
        width += points(YGNodeStyleGetWidth(c)) + points(YGNodeStyleGetMargin(c, YGEdgeLeft))
                 + points(YGNodeStyleGetMargin(c, YGEdgeRight));
    }
    YGNodeStyleSetFlexBasis(node, width);
    YGNodeStyleSetFlexShrink(node, 0);
    button->invalidate();
}

void fit_head(brls::Box* row)
{
    if (!row || row->getChildren().size() < 2) return;
    auto* title = dynamic_cast<brls::Label*>(row->getChildren()[0]);
    auto* state = dynamic_cast<brls::Label*>(row->getChildren()[1]);
    if (!title || !state) return;
    wrap(row);
    YGNodeStyleSetGap(row->getYGNode(), YGGutterColumn, 8);
    YGNodeStyleSetGap(row->getYGNode(), YGGutterRow, 2);
    title->setShrink(0.0f);
    state->setSingleLine(true);
    state->setShrink(0.0f);
}

void draw_more_below(NVGcontext* vg, brls::ScrollingFrame* frame, float x, float y, float width, float height,
                     const char* color_key, float corner)
{
    // The content: the frame's tallest child (the other is its scroll bar).
    brls::View* content = nullptr;
    if (frame)
        for (brls::View* child : frame->getChildren())
            if (!content || child->getHeight() > content->getHeight()) content = child;
    if (!content) return;
    const float below = content->getHeight() - frame->getContentOffsetY() - height;
    if (below <= 1.0f) return;
    constexpr float FADE = 32.0f;
    NVGcolor solid = brls::Application::getTheme()[color_key];
    solid.a *= frame->getAlpha();
    NVGcolor clear = solid;
    clear.a = 0.0f;
    const float top = y + height - FADE;
    nvgBeginPath(vg);
    if (corner > 0.0f)
        nvgRoundedRectVarying(vg, x + 1, top, width - 2, FADE - 1, 0, 0, corner - 1, corner - 1);
    else
        nvgRect(vg, x, top, width, FADE);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, top, x, y + height, clear, solid));
    nvgFill(vg);
}

std::string without_chevron(const std::string& value)
{
    static const std::string chevron = "›";
    if (value.size() < chevron.size() || value.compare(value.size() - chevron.size(), chevron.size(), chevron) != 0)
        return value;
    std::string out = value.substr(0, value.size() - chevron.size());
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

const char* dot_of(NVGcolor c)
{
    const auto same = [&c](NVGcolor o) { return c.r == o.r && c.g == o.g && c.b == o.b; };
    if (same(cadran::kit::color("cadran/teal"))) return "cadran/teal_fill";
    if (same(cadran::kit::color("cadran/amber"))) return "cadran/amber_fill";
    if (same(cadran::kit::color("cadran/coral"))) return "cadran/coral_fill";
    return "cadran/text3";
}

namespace
{
// {0} the day's name, {1} the same in lower case, {2} the day, {3} the month.
std::string date_text(int weekday, int day, int month)
{
    return brls::getStr("playshelf/cadran/date", brls::getStr(fmt::format("playshelf/days/{}", weekday)),
                        brls::getStr(fmt::format("playshelf/days_lower/{}", weekday)), day,
                        brls::getStr(fmt::format("playshelf/cadran/months/{}", month)));
}
}   // namespace

std::string long_date()
{
    const std::string date = ui::today_date();   // "2026-10-08"
    int month = 0, day = 0;
    if (date.size() >= 10) {
        month = std::atoi(date.substr(5, 2).c_str());
        day = std::atoi(date.substr(8, 2).c_str());
    }
    if (month < 1 || month > 12 || day < 1) return date;
    return date_text(ui::today_weekday(), day, month);
}

std::string long_date(const std::string& ymd)
{
    if (ymd.size() != 10 || ymd[4] != '-' || ymd[7] != '-') return ymd;
    std::tm tm = {};
    tm.tm_year = std::atoi(ymd.substr(0, 4).c_str()) - 1900;
    tm.tm_mon = std::atoi(ymd.substr(5, 2).c_str()) - 1;
    tm.tm_mday = std::atoi(ymd.substr(8, 2).c_str());
    tm.tm_hour = 12;   // clear of any change of hour
    if (tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1 || std::mktime(&tm) == (std::time_t)-1) return ymd;
    return date_text(tm.tm_wday, tm.tm_mday, tm.tm_mon + 1);
}

}   // namespace cadran::kit
