// cadran — the Cadran look of borealis' dialogs and pickers: a rounded card
// on a navy backdrop, the buttons as pills (a confirmation in teal, or coral
// when it deletes or stops something), the picker floating above the hints
// with its current row in teal. The toasts and the backdrop are theme values
// (cadran::install_theme).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ui/cadran_kit.hpp"

namespace cadran
{
// Under a dressed dialog's content: its button row (a 56 px pill and 24 px
// below), and what the card keeps from the screen's top and bottom.
constexpr float DIALOG_BUTTONS = 80;
constexpr float DIALOG_MARGIN  = 24;
// A picker's card: the radius of its corners.
constexpr float PICKER_CORNER = 24;   // kit::RADIUS_MODAL

enum class Emphasis
{
    NONE,      // OK in teal, any other button plain
    PRIMARY,   // the last button is the confirmation: teal
    DANGER,    // the same, in coral
    FAILURE,   // something asked did not happen: the coral warning before the
               // title, the buttons as NONE's
};

// A paragraph of the app's that dialog_body() draws as a note, with `icon`
// (in amber with `warn`): one whose text starts as the text of the i18n key
// `key` does, up to its first "{}". The note kinds are the app's: added once
// at start-up.
void add_note(const std::string& key, const char* icon, bool warn = false);

// A dialog's text laid out as the boards do (ui::dialog), left-aligned: its
// question as a title, a paragraph made with cadran_text::table as a table
// (its mark says so, not its words), the notes the app adds (add_note) at
// the bottom, small, each with its icon; any other paragraph as text.
brls::Box* dialog_body(const std::string& text);

// A dialog's title as dialog_body sets it (the display face, 23 px).
brls::Label* dialog_title(const std::string& text);
// A round badge before a title: `icon` in `color_key` on `soft_key`.
brls::Box* dialog_badge(const char* icon, const char* color_key, const char* soft_key);

// `dialog` as a card, its buttons as pills. Once: a dressed dialog is left as
// it is (every screen that takes the focus comes through here, see
// install_screens). Call it after the dialog's last addButton. DANGER and
// FAILURE also put a coral warning before a dialog_body's title.
void style_dialog(brls::Dialog* dialog, Emphasis emphasis = Emphasis::NONE);

// A button of a dialog's own, not of its row (one that must act before the
// dialog closes): a pill as style_dialog draws them, plain unless PRIMARY or
// DANGER.
void style_button(brls::Button* button, Emphasis emphasis = Emphasis::NONE);

// A dialog's body while something is under way: a spinner, and what is being
// done under it, centred.
brls::Box* wait_body(const std::string& text);

// A dialog's body while something measurable is under way, left-aligned: a
// head (an image, hidden until it is given one; what is under way, small;
// a title cut to one line), a bar, the share done under its start and what it amounts to under
// its end, a note under them (hidden when empty). What comes after (a
// button) goes on `box`.
struct Progress
{
    brls::Box*   box     = nullptr;
    brls::Image* image   = nullptr;
    brls::Label* heading = nullptr;
    brls::Label* title   = nullptr;
    kit::Bar*    bar     = nullptr;
    brls::Label* percent = nullptr;
    brls::Label* detail  = nullptr;

    // `fraction` (0..1) on the bar and as "43 %", `detail` at the end.
    void show(float fraction, const std::string& detail);
};
Progress progress_body(const std::string& heading, const std::string& title, const std::string& note = "");

// A time of day chosen with the stick among `times` (minutes after midnight,
// in order, evenly apart): up / down the next or the one before, L / R an
// hour (or a tap on the arrows), shown from the first at or after `initial`;
// OK gives `on_value` the time. `off` (when it has an action) is the button
// before OK, else Cancel; `other` (when it has one) is on Y, shown with the
// keys (a tap works too): the dialog closes, then it runs. B cancels.
struct TimeButton
{
    std::string label;
    std::function<void()> action;
};
void prompt_time(const std::string& header, std::vector<int> times, int initial, TimeButton off, TimeButton other,
                 std::function<void(int)> on_value);

// A toast's icon: a check for something done, a warning for something not
// done.
brls::View* toast_icon(bool done);

}   // namespace cadran
