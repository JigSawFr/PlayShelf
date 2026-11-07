// HomebrewTab — the homebrew a library server offers (SPEC.md § 8, action/
// homebrew_flow): one line each (name, version, author), saying when the
// console has it or an update. A on one: install it, update it or remove it
// (a path that needs consent is asked first). Ⓧ reads the server again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>

#include "action/homebrew_flow.hpp"
#include "tab/tab_base.hpp"

class HomebrewTab : public TabBase
{
  public:
    HomebrewTab();
    static aurore::View* create();

  protected:
    void refresh() override;

  private:
    void load();
    void show(const std::shared_ptr<const homebrew_flow::Loaded>& data, const std::string& why);
    void open_item(size_t index);

    std::shared_ptr<const homebrew_flow::Loaded> data;
    bool        waiting = false;
    std::string why_failed, seen;

    AURORE_BIND(aurore::Label, status, "hb_status");
    AURORE_BIND(aurore::Box,   list,   "hb_list");
};
