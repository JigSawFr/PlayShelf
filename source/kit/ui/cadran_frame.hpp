// Cadran — PlayShelf's look after the 2026 design boards: the navy and
// cool-white palettes, an icon rail for a sidebar, the mark and a status pill
// in the header, cards in place of lists, the figures in Sora. These set up
// the frame every screen shares; the tabs draw their own boards
// (source/tab/), cadran::List dresses the lists, ui/cadran_kit and
// ui/cadran_modal hold the pieces.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

namespace cadran
{
// The boards' two palettes on borealis' two variants (dark: navy; light:
// cool white), the rail's metrics and the display font: before any view.
void install_theme();
// A frame's header after the boards: the mark for the app icon, the title
// at 24, a pill at the right (`icon` and `text`; `filled` teal, else a
// bordered one) and, when asked, the date.
void style_header(brls::AppletFrame* frame, const char* icon, const std::string& text, bool filled, bool with_date);
// From then on, every screen but the main one gets the header after the
// boards (the mark, the title) and its list in cards (cadran::List), the
// first time it takes the focus.
void install_screens();
// A TabFrame's sidebar as the boards' rail: an icon before each label, the
// active tab on a teal pill.
void style_rail(brls::View* tab_frame);
// Both, for the main screen.
void style_frame(brls::AppletFrame* frame, brls::View* tab_frame);
// The main screen's header pill: its text, or hidden when empty (each
// screen says what it has to: "Parental controls on", "Play timer on"…),
// its icon, and its tone: amber for a state that needs the parent ("Pas de
// code PIN"), coral for parental controls off.
// While parental controls are known to be unlocked (ui::known_unlocked), it
// says that instead, whatever the screen, and a tap goes where that is undone.
enum class Tone { OK, WARN, DANGER };
void set_pill(const std::string& text, const char* icon = "check", Tone tone = Tone::OK);
// The pill again after the unlocked state changed (MainActivity::update_title).
void refresh_pill();
// The soft glow of the boards, an absolute full-screen view to add first in
// a frame: a radial gradient of `color_key` centred on (cx, cy), `radius`
// wide. Nothing with the lighter effects (ui/effects) or with
// PLAYSHELF_SIM_CADRAN_NOGLOW=1.
brls::View* make_glow(float cx, float cy, float radius, const char* color_key);
}
