// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/cadran_list.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <yoga/Yoga.h>

#include "core/sim_env.h"
#include "ui/cadran_kit.hpp"
#include "ui/effects.hpp"
#include "ui/motion.hpp"
#include "util/config.hpp"
#include "view/cadran_icon.hpp"

namespace kit = cadran::kit;

namespace
{
// A row's inner margins in a card: left with its icon, left without, right;
// the icon's square and the space after it.
constexpr float ROW_PAD_ICON = 20.0f;
constexpr float ROW_PAD      = 24.0f;
constexpr float ROW_PAD_END  = 24.0f;
constexpr float ICON_SQUARE  = 36.0f;
constexpr float ICON_GAP     = 14.0f;
// An action tile's icon, before its title, and the space after it.
constexpr float ACTION_ICON     = 24.0f;
constexpr float ACTION_ICON_GAP = 14.0f;

void append_utf8(std::string& out, uint32_t cp)
{
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

uint32_t upper_cp(uint32_t cp)
{
    if (cp >= 'a' && cp <= 'z') return cp - 0x20;
    if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) return cp - 0x20;   // à…þ
    if (cp == 0xFF) return 0x178;                                    // ÿ
    if (cp == 0x153) return 0x152;                                   // œ
    if (cp >= 0x430 && cp <= 0x44F) return cp - 0x20;                // а…я
    if (cp >= 0x450 && cp <= 0x45F) return cp - 0x50;                // ѐ…џ
    return cp;
}

brls::ScrollingFrame* find_scroll(brls::View* view)
{
    if (auto* frame = dynamic_cast<brls::ScrollingFrame*>(view)) return frame;
    if (auto* box = dynamic_cast<brls::Box*>(view))
        for (brls::View* child : box->getChildren())
            if (brls::ScrollingFrame* frame = find_scroll(child)) return frame;
    return nullptr;
}

int index_in(brls::Box* parent, brls::View* child)
{
    const auto& children = parent->getChildren();
    const auto at = std::find(children.begin(), children.end(), child);
    return at == children.end() ? -1 : (int)(at - children.begin());
}

float margin_top(brls::View* view)
{
    const YGValue margin = YGNodeStyleGetMargin(view->getYGNode(), YGEdgeTop);
    return margin.unit == YGUnitPoint ? margin.value : 0.0f;
}

float margin_bottom(brls::View* view)
{
    const YGValue margin = YGNodeStyleGetMargin(view->getYGNode(), YGEdgeBottom);
    return margin.unit == YGUnitPoint ? margin.value : 0.0f;
}

// A row of a card that starts with its icon (row_icon(): the only box among a cell's lines).
bool has_icon(brls::View* row)
{
    auto* box = dynamic_cast<brls::Box*>(row);
    return box && !box->getChildren().empty() && dynamic_cast<brls::Box*>(box->getChildren()[0]);
}

// Moves a line to another box, keeping the view.
void move_to(brls::View* view, brls::Box* to)
{
    if (brls::Box* from = view->getParent()) from->removeView(view, false);
    to->addView(view);
}

CadranIcon* make_icon(const char* name, float size, NVGcolor color)
{
    auto* icon = new CadranIcon();
    icon->setIcon(name);
    icon->setWidth(size);
    icon->setHeight(size);
    icon->setColor(color);
    return icon;
}

// Behind an action tile's icon and title: two soft lights in its card, as if
// blurred, its own ink near the icon and the next tile's in the far corner,
// brighter while it has the focus. Gradients, not a blur: a few fills, no
// texture. Off with the screens' background glow (lighter effects on the
// battery, Developer › glow, PLAYSHELF_SIM_CADRAN_NOGLOW).
class TileGlow : public brls::View
{
  public:
    TileGlow(brls::View* cell, NVGcolor own, NVGcolor next)
        : tile(cell), ink(own), far(next)
    {
        this->setFocusable(false);
        this->setPositionType(brls::PositionType::ABSOLUTE);
        this->setPositionTop(0);
        this->setPositionLeft(0);
    }
    void draw(NVGcontext* vg, float x, float y, float, float, brls::Style, brls::FrameContext*) override
    {
        // At the tile's corner (top and left 0), as large as the tile: a
        // percentage of it does not follow its height.
        const float width = this->tile->getWidth(), height = this->tile->getHeight();
        static const bool off = sim_env_flag("PLAYSHELF_SIM_CADRAN_NOGLOW");
        if (off || !config::get().cadran_glow || effects::light() || width <= 0 || height <= 0) return;
        // Fainter on the light theme's white, where a tint shows more.
        const bool light = brls::Application::getThemeVariant() == brls::ThemeVariant::LIGHT;
        const float lift = this->tile->isFocused() ? 1.5f : 1.0f;
        light_at(vg, x, y, width, height, x + width * 0.12f, y + height * 0.1f, width * 0.9f, this->ink, (light ? 0.14f : 0.28f) * lift);
        light_at(vg, x, y, width, height, x + width, y + height, width * 0.75f, this->far, (light ? 0.10f : 0.18f) * lift);
    }

  private:
    // One light, clipped by the card's own rounded shape.
    static void light_at(NVGcontext* vg, float x, float y, float width, float height, float cx, float cy, float radius, NVGcolor color, float alpha)
    {
        NVGcolor inner = color, outer = color;
        inner.a = std::min(alpha, 1.0f);
        outer.a = 0;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, width, height, kit::RADIUS_CARD);
        nvgFillPaint(vg, nvgRadialGradient(vg, cx, cy, 0.0f, radius, inner, outer));
        nvgFill(vg);
    }

    brls::View* tile;
    NVGcolor ink, far;
};

// The tiles' inks, one per tile in turn, never the warnings' (amber, coral):
// an action tile's icon and light, a status tile's icon.
constexpr const char* TILE_INKS[] = { "cadran/teal", "cadran/blue", "cadran/violet", "cadran/pink" };

// An action tile's lines, each on its one: its title and every label under
// it (its value, a notice's reason: TabBase::mark()). A text too long for
// its line gets smaller, to the size the line holds, down to 3/4 of its own,
// then ends in "…" and scrolls while the tile has the focus. A new text, or
// a tile of another width, starts again from the full size. Drawn as
// nothing (no size), once the tile is laid out.
class TileFit : public brls::View
{
  public:
    TileFit(brls::Box* cell, brls::Label* its_title)
        : tile(cell), title(its_title)
    {
        this->setFocusable(false);
        this->setPositionType(brls::PositionType::ABSOLUTE);
        this->setWidth(0);
        this->setHeight(0);
    }
    void draw(NVGcontext* vg, float, float, float, float, brls::Style, brls::FrameContext*) override
    {
        // The labels as they are now: one added later (a reason) joins in.
        std::vector<brls::Label*> labels = { this->title };
        for (brls::View* child : this->tile->getChildren())
            if (auto* label = dynamic_cast<brls::Label*>(child)) labels.push_back(label);
        for (brls::Label* label : labels)
            if (std::none_of(this->fits.begin(), this->fits.end(), [label](const Fit& f) { return f.label == label; })) {
                label->setSingleLine(true);
                label->setAutoAnimate(true);
                this->fits.push_back({ label, label->getFontSize(), "" });
            }
        // A wider tile (smaller text, the window) may take the full size again.
        const bool resized = this->tile->getWidth() != this->width;
        this->width = this->tile->getWidth();
        for (Fit& fit : this->fits) {
            const std::string text = fit.label->getFullText();
            if (text != fit.text || resized) {
                fit.text = text;
                if (fit.label->getFontSize() != fit.size) fit.label->setFontSize(fit.size);
                continue;   // laid out again first
            }
            const float least = fit.size * 0.75f;
            if (fit.label->getWidth() <= 0 || !fit.label->isTruncated() || fit.label->getFontSize() <= least + 0.5f) continue;
            // At once to the size its line holds (its text measured as the
            // label measures it), then a step at a time if it still does not.
            float size = fit.label->getFontSize() - 1.0f;
            if (fit.label->getFontSize() == fit.size) {
                nvgFontSize(vg, fit.label->getDrawnFontSize());
                nvgFontFaceId(vg, fit.label->getFont());
                float bounds[4];
                nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, bounds);
                const float needed = bounds[2] - bounds[0];
                if (needed > 0) size = std::floor(fit.size * fit.label->getWidth() / needed);
            }
            fit.label->setFontSize(std::max(least, std::min(size, fit.label->getFontSize() - 1.0f)));
        }
    }

  private:
    struct Fit
    {
        brls::Label* label;
        float size;         // its own size, the largest
        std::string text;   // the text it was fitted to
    };
    brls::Box* tile;
    brls::Label* title;
    std::vector<Fit> fits;
    float width = 0;   // the tile's, when last fitted
};

// The icon before a row's title, in a soft rounded square.
brls::Box* row_icon(const char* name)
{
    brls::Theme theme = brls::Application::getTheme();
    auto* square = new brls::Box(brls::Axis::ROW);
    square->setWidth(ICON_SQUARE);
    square->setHeight(ICON_SQUARE);
    square->setCornerRadius(10);
    square->setShrink(0);
    square->setBackgroundColor(theme["cadran/surf2"]);
    square->setAlignItems(brls::AlignItems::CENTER);
    square->setJustifyContent(brls::JustifyContent::CENTER);
    square->setMarginRight(ICON_GAP);
    square->addView(make_icon(name, 19, theme["cadran/text2"]));
    return square;
}
}   // namespace

std::string cadran::upper(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = (unsigned char)text[i];
        if (c < 0x80) {
            append_utf8(out, upper_cp(c));
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            append_utf8(out, upper_cp(((uint32_t)(c & 0x1F) << 6) | ((unsigned char)text[i + 1] & 0x3F)));
            i += 2;
        } else {   // 3- and 4-byte sequences (CJK, symbols): unchanged
            const size_t n = (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
            out.append(text, i, n);
            i += n;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------

cadran::Card::Card()
    : brls::Box(brls::Axis::COLUMN)
{
    brls::Theme theme = brls::Application::getTheme();
    this->fill = theme["cadran/surf"];
    this->border = theme["cadran/line"];
    this->setAlignItems(brls::AlignItems::STRETCH);
}

void cadran::Card::setAccent(NVGcolor color) { this->border = color; }
void cadran::Card::setFill(NVGcolor color) { this->fill = color; }

void cadran::Card::follow_rows()
{
    bool shown = false;
    for (brls::View* row : this->getChildren()) shown |= row->getVisibility() != brls::Visibility::GONE;
    const brls::Visibility v = shown ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
    if (this->getVisibility() != v) this->setVisibility(v);
}

void cadran::Card::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx)
{
    if (height < 1) return;   // every row hidden
    // Its rows hidden, its foot (a note) still shown: the note alone, no card.
    bool rows = false;
    for (brls::View* row : this->getChildren())
        rows |= row->getVisibility() != brls::Visibility::GONE && !dynamic_cast<brls::Label*>(row);
    if (!rows) {
        brls::Box::draw(vg, x, y, width, height, style, ctx);
        return;
    }
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f, y + 0.5f, width - 1, height - 1, kit::RADIUS_CARD);
    nvgFillColor(vg, this->a(this->fill));
    nvgFill(vg);
    nvgStrokeColor(vg, this->a(this->border));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
    // A line between the visible rows, from the title (after the icon) to the end.
    brls::View* previous = nullptr;
    for (brls::View* row : this->getChildren()) {
        if (row->getVisibility() == brls::Visibility::GONE) continue;
        // No line above a card's foot (a note): it belongs to the rows.
        if (previous && !dynamic_cast<brls::Label*>(row)) {
            const float inset = has_icon(previous) ? ROW_PAD_ICON + ICON_SQUARE + ICON_GAP : ROW_PAD;
            nvgBeginPath(vg);
            nvgRect(vg, x + inset, row->getY() - 0.5f, width - inset - ROW_PAD_END, 1);
            nvgFillColor(vg, this->a(this->border));
            nvgFill(vg);
        }
        previous = row;
    }
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

// ---------------------------------------------------------------------------

cadran::Switch::Switch(brls::BooleanCell* boolean_cell)
    : cell(boolean_cell)
{
    brls::Theme theme = brls::Application::getTheme();
    this->on = theme["cadran/teal_fill"];
    this->off = theme["cadran/track"];
    // A white knob either way: the track's colour says on or off.
    this->knob_on = theme["cadran/knob"];
    this->knob_off = theme["cadran/knob"];
    this->setFocusable(false);
    this->setWidth(50);
    this->setHeight(28);
    this->setShrink(0);
}

void cadran::Switch::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*)
{
    const bool state = this->cell->isOn();
    const float r = height / 2;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, r);
    nvgFillColor(vg, this->a(state ? this->on : this->off));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, state ? x + width - r : x + r, y + r, r - 4);
    nvgFillColor(vg, this->a(state ? this->knob_on : this->knob_off));
    nvgFill(vg);
    // Off on a light track, a hairline keeps the knob's edge.
    if (!state) {
        nvgStrokeColor(vg, this->a(brls::Application::getTheme()["cadran/knob_line"]));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }
}

// ---------------------------------------------------------------------------

cadran::List::List(brls::View* screen, bool sub_screen)
    : tab(screen)
{
    if (brls::ScrollingFrame* frame = find_scroll(screen))
        for (brls::View* child : frame->getChildren())
            if (auto* column = dynamic_cast<brls::Box*>(child)) this->box = column;
    if (!this->box) return;
    if (sub_screen) this->box->setPadding(20, 72, 36, 72);
    else this->box->setPadding(16, 48, 36, 32);
}

cadran::List::List(brls::View* screen, brls::Box* column)
    : tab(screen), box(column)
{
}

void cadran::List::state(const char* id, const char* color_key)
{
    for (auto& [cell, view] : this->marks) {
        if (cell != this->tab->getView(id)) continue;
        auto* mark = static_cast<kit::Mark*>(view);
        if (color_key) mark->set(color_key);
        mark->setVisibility(color_key ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
}

void cadran::List::tag(const char* id, const std::string& text, bool warn, const std::string& note)
{
    brls::Theme theme = brls::Application::getTheme();
    for (auto& [cell, pill] : this->tags) {
        if (cell != this->tab->getView(id)) continue;
        auto* words = static_cast<brls::Label*>(pill->getChildren()[0]);
        words->setText(text);
        words->setTextColor(theme[warn ? "cadran/amber" : "cadran/pill_ok_ink"]);
        pill->setBackgroundColor(theme[warn ? "cadran/amber_surf" : "cadran/pill_ok"]);
        pill->setBorderColor(theme[warn ? "cadran/amber_line" : "cadran/none"]);
        pill->setVisibility(text.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        cell->setBorderColor(theme[warn && !text.empty() ? "cadran/amber_line" : "cadran/line"]);
        // The note: the view right after the pill (tiles()).
        const auto& children = cell->getChildren();
        const auto at = std::find(children.begin(), children.end(), pill);
        if (at != children.end() && at + 1 != children.end())
            if (auto* under = dynamic_cast<brls::Label*>(*(at + 1))) {
                under->setText(note);
                under->setVisibility(note.empty() || text.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
            }
    }
}

brls::Box* cadran::List::head(const char* id) const
{
    for (const auto& [cell, head] : this->heads)
        if (cell == this->tab->getView(id)) return head;
    return nullptr;
}

brls::Box* cadran::List::tiles(const std::vector<std::pair<const char*, const char*>>& cells, bool status)
{
    if (!this->box || cells.empty()) return nullptr;
    brls::Theme theme = brls::Application::getTheme();
    brls::View* first = this->tab->getView(cells[0].first);
    const int at = first ? index_in(this->box, first) : -1;
    if (at < 0) return nullptr;
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::STRETCH);
    row->setMarginBottom(12);
    // Spaced by the row, not by the tiles' margins: a hidden tile (hekate
    // without its update.bin) leaves no gap behind.
    YGNodeStyleSetGap(row->getYGNode(), YGGutterColumn, 12);
    this->box->addView(row, at);
    for (size_t i = 0; i < cells.size(); i++) {
        auto* cell = dynamic_cast<brls::DetailCell*>(this->tab->getView(cells[i].first));
        if (!cell) continue;
        move_to(cell, row);
        cell->setAxis(brls::Axis::COLUMN);
        cell->setAlignItems(brls::AlignItems::STRETCH);
        cell->setJustifyContent(brls::JustifyContent::FLEX_START);
        cell->setGrow(1.0f);
        cell->setWidth(0);
        // At least this tall, taller when a title takes two lines (larger
        // text); an action tile as tall as its line (and its words, if any).
        cell->setHeight(brls::View::AUTO);
        cell->setMinHeight(status ? 128 : 0);
        if (status) this->readings.push_back(cell);
        if (status) cell->setPadding(20, 22, 18, 22);
        else cell->setPadding(17, 20, 15, 20);
        cell->setMarginRight(0);
        cell->setCornerRadius(kit::RADIUS_CARD);
        cell->setHighlightCornerRadius(kit::RADIUS_CARD);
        cell->setBackgroundColor(theme["cadran/surf"]);
        cell->setBorderColor(theme["cadran/line"]);
        cell->setBorderThickness(1);
        cell->setLineBottom(0);
        // The icon on its own line above a status tile's title; before an
        // action tile's, on the same line.
        auto* head = new brls::Box(brls::Axis::ROW);
        head->setAlignItems(brls::AlignItems::CENTER);
        head->setMarginBottom(status ? 10 : 0);
        // A tile's ink: its icon, and an action tile's light (TileGlow).
        const size_t inks = sizeof(TILE_INKS) / sizeof(TILE_INKS[0]);
        const char* ink = TILE_INKS[i % inks];
        brls::View* icon = make_icon(cells[i].second, ACTION_ICON, theme[ink]);
        head->addView(icon);
        cell->addView(head, 0);
        this->heads.push_back({ cell, head });
        if (!status) {
            cell->addView(new TileGlow(cell, theme[ink], theme[TILE_INKS[(i + 1) % inks]]), 0);
            icon->setMarginRight(ACTION_ICON_GAP);
            cell->removeView(cell->title, false);
            head->addView(cell->title);
            cell->title->setShrink(1.0f);
        }
        cell->title->setGrow(0);
        cell->title->setMarginRight(0);
        cell->title->setSingleLine(false);
        cell->title->setFontSize(status ? kit::TEXT_META : kit::TEXT_BODY);
        if (status) cell->title->setTextColor(theme["cadran/text3"]);
        cell->detail->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        cell->detail->setMaxWidth(10000);
        cell->detail->setFontSize(status ? 22.0f : kit::TEXT_META);
        cell->detail->setMarginTop(status ? 4 : 2);
        // A status tile's value is a figure, on one line; another's words
        // too (TileFit).
        cell->detail->setSingleLine(status);
        if (!status) {
            // Under the title, from where it starts: past the icon.
            cell->detail->setMarginLeft(ACTION_ICON + ACTION_ICON_GAP);
            cell->detail->setTextColor(theme["cadran/text2"]);
            cell->addView(new TileFit(cell, cell->title));
        }
        if (status) {
            // Its value after a mark of its state (state()), hidden until set.
            auto* value = new brls::Box(brls::Axis::ROW);
            value->setAlignItems(brls::AlignItems::CENTER);
            value->setMarginTop(4);
            cell->detail->setMarginTop(0);
            auto* mark = new kit::Mark("cadran/text3");
            mark->setMarginRight(10);
            mark->setShrink(0);
            mark->setVisibility(brls::Visibility::GONE);
            const int detail_at = index_in(cell, cell->detail);
            cell->removeView(cell->detail, false);
            value->addView(mark);
            value->addView(cell->detail);
            cell->addView(value, detail_at);
            this->marks.push_back({ cell, mark });
            // Its pill under the value (tag()), hidden until set: as wide as
            // its words, which wrap rather than end in "…".
            auto* pill = new brls::Box(brls::Axis::ROW);
            pill->setAlignSelf(brls::AlignSelf::FLEX_START);
            pill->setAlignItems(brls::AlignItems::CENTER);
            pill->setMarginTop(10);
            pill->setPadding(4, 12, 5, 12);
            pill->setCornerRadius(13);
            pill->setBorderThickness(1);
            pill->setVisibility(brls::Visibility::GONE);
            brls::Label* words = kit::label("", kit::TEXT_META, "cadran/text2");
            words->setSingleLine(false);
            pill->addView(words);
            cell->addView(pill, detail_at + 1);
            brls::Label* note = kit::label("", kit::TEXT_META, "cadran/text3");
            note->setSingleLine(false);
            note->setMarginTop(6);
            note->setVisibility(brls::Visibility::GONE);
            cell->addView(note, detail_at + 2);
            this->tags.push_back({ cell, pill });
        }
    }
    return row;
}

void cadran::List::apply(const std::vector<std::pair<const char*, const char*>>& icons)
{
    if (!this->box) return;
    // Marks the column as dressed (the screens' skin leaves it alone).
    auto* mark = new brls::Box();
    mark->setId("cadran_list");
    mark->setVisibility(brls::Visibility::GONE);
    this->box->addView(mark);
    const auto icon_of = [this, &icons](brls::View* cell) -> const char* {
        for (const auto& [id, icon] : icons)
            if (this->tab->getView(id) == cell) return icon;
        return nullptr;
    };

    Card* card = nullptr;   // the card the next cell joins, while cells follow each other
    const std::vector<brls::View*> lines = this->box->getChildren();
    for (brls::View* line : lines) {
        Card* above = card;   // the card just before this line, if any
        if (auto* header = dynamic_cast<brls::Header*>(line)) {
            card = nullptr;
            this->headers.push_back(header);
            section(header);
            continue;
        }
        auto* cell = dynamic_cast<brls::DetailCell*>(line);
        if (!cell) {
            card = nullptr;
            // A note (a headline, larger, keeps its size).
            if (auto* note = dynamic_cast<brls::Label*>(line); note && note->getFontSize() <= 20.5f) {
                note->setFontSize(kit::TEXT_NOTE);
                note->setMarginLeft(0);
                note->setMarginRight(0);
                if (margin_top(note) >= 10) note->setMarginTop(10);
                // Right under a card it explains: the card's foot, in it,
                // so it reads as that card's (and the next card stays apart).
                if (above) {
                    const float below = std::max(margin_bottom(note), 18.0f);
                    note->setMarginTop(2);
                    note->setMarginBottom(0);
                    note->setMargins(2, ROW_PAD_END, 16, ROW_PAD);
                    note->setTextColor(brls::Application::getTheme()["cadran/text3"]);
                    move_to(note, above);
                    above->setMarginBottom(std::max(margin_bottom(above), below));
                }
            }
            continue;
        }
        // A cell set apart from the one above (a top margin, as Advanced
        // under the extra time) starts a card of its own: in the same card,
        // the margin is an empty band across it.
        if (card && margin_top(cell) > 0) card = nullptr;
        if (!card) {
            card = new Card();
            card->setMarginTop(margin_top(cell));
            cell->setMarginTop(0);
            this->box->addView(card, index_in(this->box, cell));
        }
        move_to(cell, card);
        row(cell, icon_of(cell));
    }
    for (brls::View* line : this->box->getChildren())
        if (auto* c = dynamic_cast<Card*>(line)) c->follow_rows();
    this->sync();
    // The screen's lines rise into place one after the other, the first
    // ones only (the rest are below the fold, or come as they are): its top
    // board, its tile rows and its cards. A kicker is a label: left as it is.
    std::vector<brls::View*> rising;
    for (brls::View* line : this->box->getChildren())
        if (dynamic_cast<brls::Box*>(line) && line->getId() != "cadran_list") rising.push_back(line);
    motion::reveal_all(rising);
}

void cadran::List::section(brls::Header* header)
{
    header->setLineBottom(0);
    header->setPaddingTop(0);
    header->setPaddingBottom(0);
    header->setMarginLeft(0);   // flush with the cards, as the boards' kickers
    header->setMarginBottom(10);
    if (margin_top(header) > 0) header->setMarginTop(26);
    if (!header->getChildren().empty()) header->getChildren()[0]->setVisibility(brls::Visibility::GONE);   // the bar
    // A kicker: read from across the room (its capitals: sync()).
    for (const char* part : { "brls/header/title", "brls/header/subtitle" })
        kit::set_kicker(dynamic_cast<brls::Label*>(header->getView(part)));
}

void cadran::List::row(brls::DetailCell* cell, const char* icon, bool alone)
{
    cell->setHeight(60);
    cell->setPaddingLeft(ROW_PAD_ICON);
    cell->setPaddingRight(ROW_PAD_END);
    cell->setLineBottom(0);
    cell->setLineTop(0);
    cell->setHighlightCornerRadius(kit::RADIUS_ROW);
    cell->title->setFontSize(18);
    cell->detail->setFontSize(kit::TEXT_NOTE);
    if (icon) cell->addView(row_icon(icon), 0);
    else cell->setPaddingLeft(ROW_PAD);
    if (auto* toggle = dynamic_cast<brls::BooleanCell*>(cell)) {
        toggle->detail->setVisibility(brls::Visibility::GONE);
        toggle->addView(new Switch(toggle));
    }
    if (alone) {
        brls::Theme theme = brls::Application::getTheme();
        cell->setCornerRadius(kit::RADIUS_ROW);
        cell->setBackgroundColor(theme["cadran/surf"]);
        cell->setBorderColor(theme["cadran/line"]);
        cell->setBorderThickness(1);
        cell->setMarginBottom(8);
    }
}

void cadran::List::row_image(brls::DetailCell* cell, const std::string& image)
{
    row(cell);
    cell->setPaddingLeft(ROW_PAD_ICON);
    // In a box as row_icon()'s square, so the rows' lines start after it too.
    auto* square = new brls::Box(brls::Axis::ROW);
    square->setWidth(ICON_SQUARE);
    square->setHeight(ICON_SQUARE);
    square->setShrink(0);
    square->setAlignItems(brls::AlignItems::CENTER);
    square->setJustifyContent(brls::JustifyContent::CENTER);
    square->setMarginRight(ICON_GAP);
    auto* picture = new brls::Image();
    picture->setScalingType(brls::ImageScalingType::FIT);
    picture->setImageFromRes(image);
    picture->setWidth(30);
    picture->setHeight(30);
    square->addView(picture);
    cell->addView(square, 0);
}

void cadran::List::recolor_icon(brls::DetailCell* cell, const char* icon, const char* color_key, const char* square_key)
{
    auto* square = cell && !cell->getChildren().empty() ? dynamic_cast<brls::Box*>(cell->getChildren()[0]) : nullptr;
    if (!square) return;   // a row without its icon
    brls::Theme theme = brls::Application::getTheme();
    square->setBackgroundColor(theme[square_key ? square_key : "cadran/surf2"]);
    for (brls::View* v : square->getChildren())
        if (auto* glyph = dynamic_cast<CadranIcon*>(v)) {
            if (icon) glyph->setIcon(icon);
            glyph->setColor(theme[color_key]);
        }
}

brls::Box* cadran::List::rows_in(brls::Box* list)
{
    if (!list) return list;
    for (brls::View* child : list->getChildren())
        if (auto* card = dynamic_cast<Card*>(child)) return card;
    auto* card = new Card();
    list->addView(card);
    return card;
}

cadran::Card* cadran::List::wrap(const char* id)
{
    brls::View* line = this->box ? this->tab->getView(id) : nullptr;
    const int at = line ? index_in(this->box, line) : -1;
    if (at < 0) return nullptr;
    auto* card = new Card();
    card->setPadding(18, 24, 18, 24);
    this->box->addView(card, at);
    move_to(line, card);
    return card;
}

cadran::Card* cadran::List::card_of(const char* id) const
{
    brls::View* view = this->tab->getView(id);
    return view ? dynamic_cast<Card*>(view->getParent()) : nullptr;
}

void cadran::List::sync()
{
    for (brls::Header* header : this->headers) {
        auto* text = dynamic_cast<brls::Label*>(header->getView("brls/header/title"));
        if (!text) continue;
        const std::string title = text->getFullText();
        const std::string capitals = upper(title);
        if (capitals != title) text->setText(capitals);
    }
    brls::Theme theme = brls::Application::getTheme();
    const NVGcolor neutral = theme["brls/list/listItem_value_color"];
    for (brls::DetailCell* cell : this->readings) {
        // A reading that also opens something: the tile is the way in, no "›".
        const std::string value = cell->detail->getFullText();
        const std::string bare = kit::without_chevron(value);
        if (bare != value) cell->detail->setText(bare);
        const NVGcolor c = cell->detail->getTextColor();
        if (c.r == neutral.r && c.g == neutral.g && c.b == neutral.b && c.a == neutral.a)
            cell->detail->setTextColor(theme["cadran/text"]);
    }
}
