// The sheets of ui.hpp: pick_actions(), pick() and pick_marked() in the
// Cadran look. A header (an image when there is one, the title, what it is
// about now), then the lines in cards, a section's kicker above each group;
// each line its icon in a soft square, its label and, at its end, what is
// set now, a switch, or the check of the value chosen; removals in coral.
// It sits where borealis' Dropdown did (880 px wide, above the hints), over
// the screen it was opened from, as tall as its lines (the rest scrolls);
// B, or a touch outside it, closes it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <algorithm>
#include <cmath>
#include <memory>
#include <yoga/Yoga.h>

#include "ui/action_sheet.hpp"
#include "ui/cadran_list.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/cadran_modal.hpp"
#include "ui/ui.hpp"
#include "view/cadran_icon.hpp"

using namespace brls::literals;

namespace ui
{

void close_sheet(std::function<void()> then)
{
    brls::Application::popActivity(brls::TransitionAnimation::FADE, [then]() {
        if (then) brls::sync(then);
    });
}

brls::Box* sheet_outside(float width, bool grow)
{
    auto* box = new brls::Box();
    if (grow) box->setGrow(1);
    else box->setWidth(width);
    box->setShrink(0);
    box->addGestureRecognizer(new brls::TapGestureRecognizer(box, []() { close_sheet(nullptr); }));
    return box;
}

namespace
{
namespace kit = cadran::kit;

constexpr float SIDE = 200;           // the panel's margins: 880 px wide
constexpr float CARD_GAP = 14;
constexpr float TILE_HEIGHT = 116;    // a grid's tiles (pick_grid())
constexpr float TILE_GAP = 12;

struct Line
{
    std::string label, detail, group;
    std::string icon;          // a Cadran icon's name; empty: none
    std::string image;         // a picture in the icon's place (pick()'s `icons`)
    Choice::Tone tone = Choice::Tone::PLAIN;
    int  toggle = -1;          // a switch: 0 off, 1 on; -1 none
    bool checked = false;      // the value chosen (pick())
    std::function<void()> run;
};

// The lines' column: once laid out, its height sizes the panel.
class Column : public brls::Box
{
  public:
    Column() : brls::Box(brls::Axis::COLUMN) {}
    std::function<void(float)> measured;
    void onLayout() override
    {
        brls::Box::onLayout();
        if (this->measured && this->getHeight() > 0) this->measured(this->getHeight());
    }
};

// The panel: its list's foot fades into it while more lines follow.
class Panel : public brls::Box
{
  public:
    Panel() : brls::Box(brls::Axis::COLUMN) {}
    brls::ScrollingFrame* frame = nullptr;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override
    {
        brls::Box::draw(vg, x, y, width, height, style, ctx);
        if (this->frame)
            kit::draw_more_below(vg, this->frame, this->frame->getX(), this->frame->getY(), this->frame->getWidth(),
                                 this->frame->getHeight(), "cadran/surf", cadran::PICKER_CORNER);
    }
};

class Sheet : public brls::Box
{
  public:
    Sheet(const std::string& title, const std::string& subtitle, int image, std::vector<Line> choices, size_t first,
          int columns = 0)
        : brls::Box(brls::Axis::COLUMN), lines(std::move(choices)), focus(first)
    {
        brls::Theme theme = brls::Application::getTheme();
        this->setId("cadran_sheet");
        this->setBackgroundColor(theme["brls/backdrop"]);
        this->setAlignItems(brls::AlignItems::STRETCH);
        this->addView(sheet_outside(0, true));

        auto* panel = new Panel();
        panel->setGrow(1);
        panel->setCornerRadius(cadran::PICKER_CORNER);
        panel->setBackgroundColor(theme["cadran/surf"]);
        panel->setBorderColor(theme["cadran/line"]);
        panel->setBorderThickness(1);
        panel->setAlignItems(brls::AlignItems::STRETCH);
        brls::Box* head = this->header(title, subtitle, image);
        panel->addView(head);

        auto* frame = new brls::ScrollingFrame();
        frame->setGrow(1);
        frame->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
        auto* column = new Column();
        column->setAlignItems(brls::AlignItems::STRETCH);
        column->setPadding(18, 28, 22, 28);
        if (columns > 0) this->grid(column, (size_t)columns);
        else {
            cadran::Card* card = nullptr;
            for (size_t i = 0; i < this->lines.size(); i++) {
                const Line& l = this->lines[i];
                // A new group: its kicker (when named) and its card.
                if (!card || l.group != this->lines[i - 1].group) {
                    if (!l.group.empty()) {
                        auto* kicker = new brls::Label();
                        kit::set_kicker(kicker, l.group);
                        kicker->setSingleLine(true);
                        kicker->setMarginLeft(6);
                        kicker->setMarginTop(card ? CARD_GAP + 4 : 0);
                        kicker->setMarginBottom(10);
                        column->addView(kicker);
                    }
                    card = new cadran::Card();
                    if (l.group.empty() && !column->getChildren().empty()) card->setMarginTop(CARD_GAP);
                    column->addView(card);
                }
                card->addView(this->line(l, i));
            }
        }
        frame->setContentView(column);
        panel->frame = frame;
        panel->addView(frame);

        // As tall as its lines, up to most of the screen; the rest scrolls.
        // A first guess, then the lines' own height once laid out.
        const float most = brls::Application::contentHeight * 0.8f;
        const float body = columns > 0
            ? (TILE_HEIGHT + TILE_GAP) * (float)((this->lines.size() + columns - 1) / columns)
            : 60.0f * (float)this->lines.size();
        panel->setHeight(std::min((subtitle.empty() ? 70.0f : 92.0f) + body + 40, most));
        column->measured = [panel, head, most](float lines_height) {
            const float want = std::min(head->getHeight() + lines_height + 2, most);
            if (std::abs(want - panel->getHeight()) > 1) brls::sync([panel, want]() { panel->setHeight(want); });
        };

        auto* row = new brls::Box(brls::Axis::ROW);
        row->setAlignItems(brls::AlignItems::STRETCH);
        row->setShrink(0);
        row->addView(sheet_outside(SIDE, false));
        row->addView(panel);
        row->addView(sheet_outside(SIDE, false));
        this->addView(row);
        this->addView(sheet_outside(20, false));   // the gap above the hints
        this->addView(new brls::BottomBar());

        this->registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) {
            close_sheet(nullptr);
            return true;
        }, false, false, brls::SOUND_BACK);
    }

    bool isTranslucent() override { return true; }

    brls::View* getDefaultFocus() override
    {
        if (this->focus < this->cells.size()) return this->cells[this->focus];
        return brls::Box::getDefaultFocus();
    }

  private:
    std::vector<Line> lines;
    size_t focus;
    std::vector<brls::View*> cells;

    brls::Box* header(const std::string& title, const std::string& subtitle, int image)
    {
        brls::Theme theme = brls::Application::getTheme();
        auto* head = new brls::Box(brls::Axis::ROW);
        head->setAlignItems(brls::AlignItems::CENTER);
        head->setPadding(subtitle.empty() ? 24 : 20, 36, subtitle.empty() ? 20 : 18, image ? 28 : 36);
        head->setLineColor(theme["cadran/line"]);
        head->setLineBottom(1);
        head->setShrink(0);
        if (image) {
            auto* icon = new brls::Image();
            icon->setWidth(48);
            icon->setHeight(48);
            icon->setCornerRadius(10);
            icon->setScalingType(brls::ImageScalingType::FIT);
            icon->innerSetImage(image);
            icon->setFreeTexture(false);   // the list's, or the icon cache's
            icon->setMarginRight(16);
            icon->setShrink(0);
            head->addView(icon);
        }
        auto* words = new brls::Box(brls::Axis::COLUMN);
        words->setGrow(1);
        words->setShrink(1);
        brls::Label* name = kit::label(title, 22, "cadran/text");
        name->setSingleLine(true);
        words->addView(name);
        if (!subtitle.empty()) {
            brls::Label* line = kit::label(subtitle, kit::TEXT_META, "cadran/text2");
            line->setMarginTop(5);
            words->addView(line);
        }
        head->addView(words);
        return head;
    }

    // The lines as tiles, `columns` to a row: the icon large above the
    // label, the value chosen in teal. Up and down keep the column.
    void grid(brls::Box* column, size_t columns)
    {
        brls::Box* row = nullptr;
        for (size_t i = 0; i < this->lines.size(); i++) {
            if (i % columns == 0) {
                row = new brls::Box(brls::Axis::ROW);
                row->setAlignItems(brls::AlignItems::STRETCH);
                if (i > 0) row->setMarginTop(TILE_GAP);
                YGNodeStyleSetGap(row->getYGNode(), YGGutterColumn, TILE_GAP);
                column->addView(row);
            }
            row->addView(this->tile(this->lines[i], i));
        }
        // The last row's empty places, so its tiles keep their width.
        for (size_t i = this->lines.size(); row && i % columns != 0; i++) {
            auto* gap = new brls::Box();
            gap->setGrow(1);
            gap->setWidth(0);
            row->addView(gap);
        }
        for (size_t i = 0; i < this->cells.size(); i++) {
            if (i >= columns) this->cells[i]->setCustomNavigationRoute(brls::FocusDirection::UP, this->cells[i - columns]);
            if (i + columns < this->cells.size())
                this->cells[i]->setCustomNavigationRoute(brls::FocusDirection::DOWN, this->cells[i + columns]);
        }
    }

    brls::DetailCell* tile(const Line& l, size_t index)
    {
        brls::Theme theme = brls::Application::getTheme();
        auto* cell = new brls::DetailCell();
        cell->setText(l.label);
        cell->setDetailText("");
        cell->detail->setVisibility(brls::Visibility::GONE);
        cell->setAxis(brls::Axis::COLUMN);
        cell->setAlignItems(brls::AlignItems::CENTER);
        cell->setJustifyContent(brls::JustifyContent::CENTER);
        cell->setGrow(1);
        cell->setWidth(0);
        cell->setHeight(TILE_HEIGHT);
        cell->setPadding(14, 6, 14, 6);
        cell->setMarginRight(0);
        cell->setLineBottom(0);
        cell->setLineTop(0);
        cell->setCornerRadius(kit::RADIUS_CARD);
        cell->setHighlightCornerRadius(kit::RADIUS_CARD);
        cell->setBackgroundColor(theme[l.checked ? "cadran/teal_soft" : "cadran/surf2"]);
        cell->title->setGrow(0);
        cell->title->setMarginRight(0);
        cell->title->setFontSize(kit::TEXT_NOTE);
        // On one line: a tile's width holds the longest name ("Einschränkungen")
        // at the larger text size.
        cell->title->setSingleLine(true);
        cell->title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        if (l.checked) cell->title->setTextColor(theme["cadran/teal"]);
        if (!l.icon.empty()) {
            CadranIcon* icon = kit::icon(l.icon.c_str(), 32, l.checked ? "cadran/teal" : "cadran/text2");
            icon->setMarginBottom(12);
            icon->setShrink(0);
            cell->addView(icon, 0);
        }
        const std::function<void()> run = l.run;
        cell->registerClickAction([run](brls::View*) {
            close_sheet(run);
            return true;
        });
        if (this->cells.size() == index) this->cells.push_back(cell);
        return cell;
    }

    brls::DetailCell* line(const Line& l, size_t index)
    {
        brls::Theme theme = brls::Application::getTheme();
        brls::DetailCell* cell;
        if (l.toggle >= 0) {
            auto* toggle = new brls::BooleanCell();
            toggle->init(l.label, l.toggle == 1, [](bool) {});
            cell = toggle;
        } else {
            cell = new brls::DetailCell();
            cell->setText(l.label);
            cell->setDetailText(l.detail);
        }
        if (!l.image.empty()) cadran::List::row_image(cell, l.image);
        else cadran::List::row(cell, l.icon.empty() ? nullptr : l.icon.c_str());
        if (l.toggle < 0) {
            cell->detail->setMaxWidth(340);
            cell->detail->setSingleLine(true);
            cell->detail->setVisibility(l.detail.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        }
        const char* tint = nullptr;
        switch (l.tone) {
            case Choice::Tone::PLAIN: break;
            case Choice::Tone::ON:
                tint = "cadran/teal";
                break;
            case Choice::Tone::WARN:
                tint = "cadran/amber";
                break;
            case Choice::Tone::DANGER:
                tint = "cadran/coral";
                cell->title->setTextColor(theme["cadran/coral"]);
                break;
        }
        if (l.toggle < 0) cell->detail->setTextColor(theme[tint ? tint : "cadran/text2"]);
        if (tint && !l.icon.empty())
            cadran::List::recolor_icon(cell, nullptr, tint,
                                       l.tone == Choice::Tone::DANGER ? "cadran/coral_soft"
                                       : l.tone == Choice::Tone::WARN ? "cadran/amber_surf"
                                                                      : "cadran/teal_soft");
        // The value chosen: a soft fill and its check at the end.
        if (l.checked) {
            cell->setBackgroundColor(theme["cadran/teal_soft"]);
            cell->setCornerRadius(kit::RADIUS_ROW);
            CadranIcon* check = kit::icon("check", 22, "cadran/teal");
            check->setMarginLeft(l.detail.empty() ? 0 : 14);
            check->setShrink(0);
            cell->addView(check);
        }
        // Chosen: the sheet goes, then the line's action (none: it only closes).
        const std::function<void()> run = l.run;
        cell->registerClickAction([run](brls::View*) {
            close_sheet(run);
            return true;
        });
        if (this->cells.size() == index) this->cells.push_back(cell);
        return cell;
    }
};

void open(const std::string& title, const std::string& subtitle, int image, std::vector<Line> lines, size_t focus,
          int columns = 0)
{
    if (lines.empty()) return;
    if (focus >= lines.size()) focus = 0;
    brls::Application::pushActivity(new brls::Activity(new Sheet(title, subtitle, image, std::move(lines), focus, columns)),
                                    brls::TransitionAnimation::FADE);
}

std::vector<Line> value_lines(const std::vector<std::string>& values, const std::vector<std::string>& details,
                              std::function<bool(size_t)> checked, std::function<void(int)> on_pick,
                              const std::vector<std::string>& icons = {})
{
    std::vector<Line> lines;
    auto pick = std::make_shared<std::function<void(int)>>(std::move(on_pick));
    for (size_t i = 0; i < values.size(); i++) {
        Line l;
        l.label = values[i];
        l.detail = i < details.size() ? details[i] : "";
        // A picture ("img/…png") or a Cadran icon's name.
        const std::string icon = i < icons.size() ? icons[i] : "";
        (icon.rfind("img/", 0) == 0 ? l.image : l.icon) = icon;
        l.checked = checked(i);
        const int index = (int)i;
        l.run = [pick, index]() {
            if (*pick) (*pick)(index);
        };
        lines.push_back(std::move(l));
    }
    return lines;
}
}   // namespace

bool is_sheet(brls::View* view)
{
    return view && view->getId() == "cadran_sheet";
}

void pick_actions(const std::string& title, const std::vector<Choice>& choices, const std::string& subtitle, int image)
{
    std::vector<Line> lines;
    for (const Choice& c : choices) {
        Line l;
        l.label = c.label;
        l.detail = c.detail;
        l.group = c.group;
        l.icon = c.icon ? c.icon : "";
        l.tone = c.tone;
        l.toggle = c.toggle;
        l.run = c.run;
        lines.push_back(std::move(l));
    }
    open(title, subtitle, image, std::move(lines), 0);
}

void pick(const std::string& title, const std::vector<std::string>& values, int selected,
          std::function<void(int)> on_pick, bool checked, const std::vector<std::string>& details,
          const std::vector<std::string>& icons)
{
    const size_t focus = selected < 0 ? 0 : (size_t)selected;
    open(title, "", 0,
         value_lines(values, details, [checked, focus](size_t i) { return checked && i == focus; }, std::move(on_pick),
                     icons),
         focus);
}

void pick_grid(const std::string& title, const std::vector<std::string>& values, const std::vector<std::string>& icons,
               int selected, std::function<void(int)> on_pick, int columns)
{
    const size_t focus = selected < 0 ? 0 : (size_t)selected;
    open(title, "", 0,
         value_lines(values, {}, [focus](size_t i) { return i == focus; }, std::move(on_pick), icons), focus,
         columns < 1 ? 1 : columns);
}

void pick_marked(const std::string& title, const std::vector<std::string>& values, const std::vector<bool>& marks,
                 int focus, std::function<void(int)> on_pick, const std::vector<std::string>& details)
{
    open(title, "", 0,
         value_lines(values, details, [marks](size_t i) { return i < marks.size() && marks[i]; }, std::move(on_pick)),
         focus < 0 ? 0 : (size_t)focus);
}

}   // namespace ui
