// CadranIcon — Cadran's icons, drawn as nanovg paths from
// the design boards' 24-unit SVG strokes (no SVG loader in this borealis):
// today, timer, activity, shield, lock, globe, settings, tools, info, plus,
// stop, eye, check, moon, pause, list, lines, download, chevron-down, chevron-left,
// chevron-right, caret-up, caret-down, and "mark", the PlayShelf logo
// (branding/mark.svg); for the other screens, in the same strokes: key,
// unlock, phone, trash, server, pin, refresh, history, restore, save, upload,
// language, contrast, home, bolt, sparkle, bell, chip, code, heart, folder,
// file, flag, gamepad, calendar, alert, pencil, pencil-off, tag, user, sun, tv,
// text-normal, text-large.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

class CadranIcon : public brls::View
{
  public:
    CadranIcon();

    void setIcon(const std::string& name);
    void setColor(NVGcolor color);
    // The icon pops into place (grows from nothing, a little past its size,
    // then settles): a check that was just earned. Nothing with the motion
    // off (ui/effects).
    void pop();

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    static brls::View* create();

  private:
    std::string icon = "today";
    NVGcolor color;
    brls::Time pop_since = 0;   // pop(): when it began (µs), 0 at rest
};
