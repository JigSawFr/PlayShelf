// UpdateActivity — "PlayShelf 1.2.0 is available": shown at start-up while a
// newer release is known (action/update_flow, unless postponed) and after
// About › Check for updates finds one. The new version's date and notes, the
// running version, whether it supports this firmware; Update, the way it
// is installed (About › Update with), Later (1 day to 30 days). B closes it:
// at start-up it comes back next time.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

#include "action/update_flow.hpp"

class UpdateActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/update.xml");

    static void show(const update_flow::Offer& offer);

    void onContentAvailable() override;

  private:
    update_flow::Offer offer;

    void refresh();

    AURORE_BIND(aurore::Label,      headline,     "up_headline");
    AURORE_BIND(aurore::Label,      body,         "up_body");
    AURORE_BIND(aurore::DetailCell, update,       "up_update");
    AURORE_BIND(aurore::DetailCell, method,       "up_method");
    AURORE_BIND(aurore::DetailCell, later,        "up_later");
    AURORE_BIND(aurore::Header,     notes_header, "up_notes_header");
    AURORE_BIND(aurore::Box,        notes,        "up_notes");
    AURORE_BIND(aurore::Label,      notes_none,   "up_notes_none");
};
