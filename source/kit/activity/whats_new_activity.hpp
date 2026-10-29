// WhatsNewActivity — "What's new in PlayShelf X.Y.Z": shown once at start-up
// after an update (action/support_flow), the running version's highlights
// and a cell to the full list (or the full list when it has none), a Close
// cell, then the funding QR codes. B closes it too.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

class WhatsNewActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/whats_new.xml");

    void onContentAvailable() override;

  private:
    AURORE_BIND(aurore::Header,         header,       "wn_header");
    AURORE_BIND(aurore::Box,            notes,        "wn_notes");
    AURORE_BIND(aurore::DetailCell,     full,         "wn_full");
    AURORE_BIND(aurore::DetailCell,     close,        "wn_close");
    AURORE_BIND(aurore::Label,          support_note, "wn_support_note");
    AURORE_BIND(aurore::Box,            funding_box,  "wn_funding");
};
