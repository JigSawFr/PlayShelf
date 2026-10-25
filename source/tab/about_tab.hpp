// AboutTab — the installed PlayShelf and help with it: version (Ⓐ opens its
// release notes, SysNotesActivity; seven presses of a hidden Ⓨ on it turn the
// developer tools on, at the end of Help & diagnostics), launch mode and data
// folder; its updates (check now, every day, and how); a line to Help & diagnostics (HelpTab, on a screen of its own); the credits; how to
// support it (the funding links as QR codes, to scan with a phone); "Made in
// France".
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "ui/cadran_list.hpp"
#include "tab/tab_base.hpp"

class AboutTab : public TabBase
{
  public:
    AboutTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    cadran::List list;
    brls::Label* version_line = nullptr;   // the top's version and commit
    int        version_presses    = 0;
    brls::Time last_version_press = 0;

    void count_version_press();

    BRLS_BIND(brls::DetailCell,  version,          "ab_version");
    BRLS_BIND(brls::DetailCell,  update_cell,      "ab_update");
    BRLS_BIND(brls::DetailCell,  help,             "ab_help");
    BRLS_BIND(brls::DetailCell,  credits,          "ab_credits");
    BRLS_BIND(brls::Label,       support_note,     "ab_support_note");
    BRLS_BIND(brls::Box,         funding,          "ab_funding");
};
