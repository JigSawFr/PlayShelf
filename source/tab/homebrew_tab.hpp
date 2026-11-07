// HomebrewTab — the homebrew a library server offers (SPEC.md § 8, action/
// homebrew_flow), as tiles like the Catalogue's (tab/homebrew_panel).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>

#include "tab/homebrew_panel.hpp"
#include "tab/tab_base.hpp"

class HomebrewTab : public TabBase
{
  public:
    HomebrewTab();
    static aurore::View* create();

  protected:
    void refresh() override;

  private:
    AURORE_BIND(aurore::Label, status, "hb_status");
    AURORE_BIND(aurore::Box,   list,   "hb_list");
    std::unique_ptr<HomebrewPanel> panel;
};
