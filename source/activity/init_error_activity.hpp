// InitErrorActivity — root Activity when what PlayShelf needs from the console cannot be opened
// (typically: no CFW). One-shot screen; B exits the app.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

class InitErrorActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/init_error.xml");

    void onContentAvailable() override;

  private:
    AURORE_BIND(aurore::Label, error_code, "error_code");
    AURORE_BIND(aurore::Label, error_fw,   "error_fw");
};
