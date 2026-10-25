// cadran::kit — the small pieces every Cadran screen builds with: a label, an
// icon, a card, a pill, a chip, an empty state; a line's value without its
// "›", a dot for a value's colour, a date in words.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

#include "view/cadran_icon.hpp"

namespace cadran::kit
{
// The boards' corners, and the XML's (@style/cadran/radius/<name>,
// cadran::install_theme): a row on its own, a table, a picker's row (ROW); a
// game's icon (ICON); a card of a list, a tile (CARD); a screen's top card
// (TOP); a dialog's and a picker's card (MODAL). A pill is round: half its
// height.
constexpr float RADIUS_ROW   = 14;
constexpr float RADIUS_ICON  = 12;
constexpr float RADIUS_CARD  = 20;
constexpr float RADIUS_TOP   = 22;
constexpr float RADIUS_MODAL = 24;

// The boards' type scale: what is said in passing (a code, a caption, a
// pill), a section's kicker in capitals, a note under the lines, a line's
// text, a dialog's paragraph, a dialog's title (in the display face).
constexpr float TEXT_META   = 15;
constexpr float TEXT_KICKER = 16;
constexpr float TEXT_NOTE   = 16.5f;
constexpr float TEXT_BODY   = 17;
constexpr float TEXT_LEAD   = 19;
constexpr float TEXT_TITLE  = 23;

// A theme colour of the boards ("cadran/teal"…), for what draws itself or
// sets a colour: always a key of the palettes, never a value of its own, so
// it follows the theme.
NVGcolor color(const char* key);

// A label of `size` px in the theme colour `color_key` ("cadran/text2"…);
// several lines when it has to.
brls::Label* label(const std::string& text, float size, const char* color_key = "cadran/text");

// The same in the boards' display face (Sora; the other scripts from the
// app's fonts): a dialog's title, a code to read out, a figure.
brls::Label* display(const std::string& text, float size, const char* color_key = "cadran/text");

// An icon of the boards, `size` px square.
CadranIcon* icon(const char* name, float size, const char* color_key = "cadran/text2");

// A card: the surface, a 1 px line, rounded; `focusable` takes the focus
// ring (no highlight fill); `grow` shares a row with its neighbours.
brls::Box* card(bool focusable, bool grow);

// A card at the top of a screen (not a list: no rows), larger, `grow`
// shares a row.
brls::Box* top_card(bool grow);
// A kicker's look, the one every section title and board's line above its
// headline takes: TEXT_KICKER, the second ink, `text` in capitals (none:
// the label's text kept, its look only).
void set_kicker(brls::Label* label, const std::string& text = "");
// A kicker with an icon before it, in a card: an icon and a title in capitals.
brls::Box* kicker(const char* icon_name, const std::string& title);

// A pill's mark: its colour, and a shape that says the same to whom the
// colours look alike (WCAG 1.4.1): a check for what is on (teal), a
// triangle for what needs the parent (amber, coral), a dot for what is only
// said (grey).
class Mark : public brls::View
{
  public:
    explicit Mark(const char* color_key);
    void set(const char* color_key);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    enum class Shape
    {
        DOT,
        CHECK,
        WARN,
    } shape = Shape::DOT;
    NVGcolor color;
};

// A pill: a mark of `dot_key` (its first child) and its text, on a thin
// border.
brls::Box* pill(const std::string& text, const char* dot_key);

// A controller key as the boards draw it: its letter ("A", "Y", "L") in a
// round bubble, `size` px across, no margins.
brls::Box* key(const char* letter, float size = 24);

// A QR code of `payload`, `side` px (dark on white whatever the theme, as
// phones read it best), its `title` under it and a `caption` smaller (either
// may be empty), centred; `width` (0: the code's own) keeps cards alike.
brls::Box* qr(const std::string& payload, float side, const std::string& title, const std::string& caption,
              float width = 0);

// A progress bar, 12 px high: the well, filled in teal up to `set`; the
// fill eases to each new value (at once with the motion off).
class Bar : public brls::View
{
  public:
    Bar();
    void set(float fraction);   // 0..1
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    float drawn() const;   // the fill now, on its way from `from` to `fraction`
    float fraction = 0;
    float from = 0;
    brls::Time since = 0;
};

// A rounded chip with a text in the display face (an amount, a period…).
// Pressed (A, a tap), it dips a little and comes back, with the press's
// own flash.
class Chip : public brls::Box
{
  public:
    Chip();
    void frame(brls::FrameContext* ctx) override;
};
brls::Box* chip(const std::string& text, float height = 48);

// What an empty list shows instead: a card with `icon_name` in a soft circle,
// `title` (why it is empty) and `text` under it (what will come; may be
// empty), centred; `box` takes an action (a chip) at its end.
struct Empty
{
    brls::Box*   box   = nullptr;
    brls::Label* title = nullptr;
    brls::Label* text  = nullptr;
};
Empty empty(const char* icon_name);
// The same around a screen's own label (its XML's, which it fills and shows):
// the label becomes the card's title, in place, and ui::set_visible on it
// shows or hides the whole card. Returns the card.
brls::Box* empty_around(brls::Label* label, const char* icon_name);

// A row whose items go on to the next line when they do not fit (the XML
// has no attribute for it).
void wrap(brls::View* row);

// A button of such a row laid out by its text at the normal size: the row
// breaks only where it would at that size, and with the larger text
// (ui/text_size) the button's text is drawn smaller again, down to its size,
// rather than pushing the button to a line of its own (patches/borealis/0007).
// Again after its text changes.
void fit_button(brls::Button* button);

// A card's head: its title, then its state on the right; the state under
// the title when a language (or the larger text) leaves no room for both,
// each whole ("Jugendschutz", not "Jugendschut" over "z").
void fit_head(brls::Box* row);

// Over the foot of a scrolling frame drawn at (x, y, width, height): the
// background (`color_key`) fading in over 32 px while there is more below,
// so a line cut by the frame's edge reads as "more", not as the end.
// `corner`: the radius of the card whose foot the frame's is (a picker's
// list), the fade then inside its rounded corners and its 1 px border.
void draw_more_below(NVGcontext* vg, brls::ScrollingFrame* frame, float x, float y, float width, float height,
                     const char* color_key, float corner = 0.0f);

// "12 changes  ›" -> "12 changes"; "›" -> "".
std::string without_chevron(const std::string& value);

// The value colour a line was given, as a dot: teal (ok), amber (warn),
// coral (bad), or the quiet one.
const char* dot_of(NVGcolor value_color);

// "jeudi 8 octobre", from the console's clock, in the app's language; the
// same for a "2026-10-08" (the text itself when it is not a date).
std::string long_date();
std::string long_date(const std::string& ymd);
}   // namespace cadran::kit
