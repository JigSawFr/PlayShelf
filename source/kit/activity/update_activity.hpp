// UpdateActivity — "PlayShelf 1.2.0 is available": shown at start-up while a
// newer release is known (action/update_flow, unless postponed) and after
// About › Check for updates finds one. The new version's date and notes, the
// running version, whether it supports this firmware; Update, the way it
// is installed (About › Update with), Later (1 day to 30 days). B closes it:
// at start-up it comes back next time.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "action/update_flow.hpp"

class UpdateActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/update.xml");

    static void show(const update_flow::Offer& offer);

    void onContentAvailable() override;

  private:
    update_flow::Offer offer;

    void refresh();

    BRLS_BIND(brls::Label,      headline,     "up_headline");
    BRLS_BIND(brls::Label,      body,         "up_body");
    BRLS_BIND(brls::DetailCell, update,       "up_update");
    BRLS_BIND(brls::DetailCell, method,       "up_method");
    BRLS_BIND(brls::DetailCell, later,        "up_later");
    BRLS_BIND(brls::Header,     notes_header, "up_notes_header");
    BRLS_BIND(brls::Box,        notes,        "up_notes");
    BRLS_BIND(brls::Label,      notes_none,   "up_notes_none");
};
