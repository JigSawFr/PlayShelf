// NoticesActivity — the notification bell's list (action/notices), as
// PlayGuard's: "To do", then "For information". A on an entry closes the
// list and runs its action (what it says, done, or the screen where it is);
// an entry for information with nothing to do is put away. X puts one for
// information away; Y every one. Rebuilt when the entries change; closes by
// itself once none is left.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <memory>

#include "action/notices.hpp"

class NoticesActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/notices.xml");
    ~NoticesActivity() override;

    void onContentAvailable() override;
    void onResume() override;

    // Y on the main screen, a tap on the bell: the list, or a toast when it
    // is empty.
    static void open();

  private:
    int  listener = 0;
    bool closing = false;
    bool stale = false;   // changed under a dialog: rebuilt once back on top
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    aurore::Box* content = nullptr;

    void rebuild();
    aurore::DetailCell* entry_cell(const notices::Entry& e, bool todo);

    AURORE_BIND(aurore::Box, root, "nt_root");
};
