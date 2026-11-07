// HomebrewTab — the homebrew a library server offers (SPEC.md § 8, action/
// homebrew_flow), as tiles like the Catalogue's: its icon from the server,
// its name, and under it its version, or that the console has it (amber when
// there is an update). A on one: install it, update it or remove it
// (a path that needs consent is asked first). Ⓧ reads the server again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>
#include <vector>

#include "action/homebrew_flow.hpp"
#include "tab/tab_base.hpp"

class GameTile;

class HomebrewTab : public TabBase
{
  public:
    HomebrewTab();
    static aurore::View* create();

  protected:
    void refresh() override;

  private:
    void load();
    void present(const std::shared_ptr<const homebrew_flow::Loaded>& data, const std::string& why);
    GameTile* make_tile(size_t index);
    void open_item(size_t index);
    void check_data(size_t index);

    std::shared_ptr<const homebrew_flow::Loaded> data;
    std::vector<GameTile*> tiles;   // the tiles shown, in the order of the entries
    int         generation = 0;     // changes when they are built again: an icon on its way finds its tile gone
    bool        waiting = false;
    std::string why_failed, seen;

    AURORE_BIND(aurore::Label, status, "hb_status");
    AURORE_BIND(aurore::Box,   list,   "hb_list");
};
