// ChangelogActivity — every change in the running version (the bundled
// CHANGELOG.md), opened from the highlights on the "What's new" screen; then where the earlier versions are. B closes it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class ChangelogActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/changelog.xml");

    void onContentAvailable() override;

  private:
    BRLS_BIND(brls::Header, header, "cl_header");
    BRLS_BIND(brls::Box,    notes,  "cl_notes");
    BRLS_BIND(brls::Label,  note,   "cl_note");
};
