// BorealeTab — About › Boréale homebrews, a screen of its own: the Boréale
// homebrews installed on the console (their registry,
// action/boreale_apps_logic), PlayShelf first, each with its version; Ⓐ
// on another one closes PlayShelf and starts it (hbloader's next load), or
// says to open it from the homebrew menu when it cannot be started from
// here (an applet, no .nro known).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "action/boreale_apps_logic.hpp"
#include "tab/tab_base.hpp"

class BorealeTab : public TabBase
{
  public:
    BorealeTab();

    // The homebrews' names, for the row that opens this screen
    // ("PlayShelf, PlayShelf").
    static std::string names();

  protected:
    void refresh() override;

  private:
    std::string seen;   // what the list shows: built again when it changes

    aurore::DetailCell* cell(const boreale_apps_logic::App& app);

    AURORE_BIND(aurore::Label, intro, "bo_intro");
    AURORE_BIND(aurore::Box,   list,  "bo_list");
};
