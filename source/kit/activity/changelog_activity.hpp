// ChangelogActivity — every change in the running version (the bundled
// CHANGELOG.md), opened from the highlights on the "What's new" screen; then where the earlier versions are. B closes it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

class ChangelogActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/changelog.xml");

    void onContentAvailable() override;

  private:
    AURORE_BIND(aurore::Header, header, "cl_header");
    AURORE_BIND(aurore::Box,    notes,  "cl_notes");
    AURORE_BIND(aurore::Label,  note,   "cl_note");
};
