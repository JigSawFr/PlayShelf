// AboutTab — the installed PlayShelf and help with it: version (Ⓐ opens its
// release notes, SysNotesActivity; seven presses of a hidden Ⓨ on it turn the
// developer tools on, at the end of Help & diagnostics), launch mode and data
// folder; its updates (check now, every day, and how); a line to Help & diagnostics (HelpTab, on a screen of its own); the credits; how to
// support it (the funding links as QR codes, to scan with a phone); "Made in
// France".
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore/cadran/ui/cadran_list.hpp>

#include "tab/tab_base.hpp"

class AboutTab : public TabBase
{
  public:
    AboutTab();
    static aurore::View* create();

  protected:
    void refresh() override;

  private:
    cadran::List list;
    aurore::Label* version_line = nullptr;   // the top's version and commit
    int        version_presses    = 0;
    aurore::Time last_version_press = 0;

    void count_version_press();

    AURORE_BIND(aurore::DetailCell,  version,          "ab_version");
    AURORE_BIND(aurore::DetailCell,  update_cell,      "ab_update");
    AURORE_BIND(aurore::DetailCell,  help,             "ab_help");
    AURORE_BIND(aurore::DetailCell,  credits,          "ab_credits");
    AURORE_BIND(aurore::Label,       support_note,     "ab_support_note");
    AURORE_BIND(aurore::Box,         funding,          "ab_funding");
};
