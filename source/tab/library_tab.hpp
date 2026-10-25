// LibraryTab — the library server's catalogue. Until a server can be set up,
// what is coming, as an empty state.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"

class LibraryTab : public TabBase
{
  public:
    LibraryTab();
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    BRLS_BIND(brls::Box, content, "lb_content");
};
