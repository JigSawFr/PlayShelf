// NoticeBell — the notification bell at the right of the main screen's
// header, as PlayGuard's: a bell drawn with nanovg, with the number of
// entries to do on an amber disc. Information only: the bell without a
// number, a small dot. Not focusable: Y opens the list (action/notices), and
// so does a tap on it. Only on the main screen, in the header's row at the
// right (VersionBadge::header_tools; the title and the version badge stay at
// the left).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

class NoticeBell : public aurore::View
{
  public:
    NoticeBell();
    ~NoticeBell() override;

    // Puts the bell in the main screen's header, when it first takes the
    // focus. Called once, at start-up.
    static void install();

    void draw(NVGcontext* vg, float x, float y, float width, float height, aurore::Style style,
              aurore::FrameContext* ctx) override;
    // Touched although it is not focusable.
    aurore::View* hitTest(aurore::Point point) override;

  private:
    int listener = 0;
    int actions  = 0;
    int infos    = 0;
    bool counted = false;        // update() ran once: a change after that moves
    aurore::Time swing_since = 0;  // a new entry to do: the bell swings (µs; 0 at rest)
    aurore::Time pop_since   = 0;  // the number changed: its disc pops

    void update();
};
