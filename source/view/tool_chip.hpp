// ToolChip — one of a tab's controls as a small pill in a row (the
// Catalogue's server, search, order, filters, section; the Library's games
// and its order and filters): its icon, what it sets in dim words, what it
// is set to. A on it runs its action. In teal when it narrows what the tab
// shows (a search, a filter). The tab's games keep the room below it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

class CadranIcon;

class ToolChip : public brls::Box
{
  public:
    ToolChip(const char* icon_name, const std::string& what);

    // What it is set to; set only when it changes (a label set lays the
    // screen out again).
    void set_value(const std::string& text);
    // In teal: it narrows what the tab shows.
    void set_active(bool on);

  private:
    CadranIcon*  icon = nullptr;
    brls::Label* what = nullptr;
    brls::Label* value = nullptr;
    bool         active = false;
};
