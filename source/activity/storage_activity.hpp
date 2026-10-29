// StorageActivity — the SD card's room (Home's Storage tile, the bell when
// it is nearly full): how much is free, a bar of it, then the console's
// games not played for three months and large ("To free"), then every game
// by the room it takes, with when it was last played. A on a game: delete
// it (asked first; its saves stay) or open its page. Rebuilt when the
// console's games are read again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <memory>

#include "action/insights.hpp"

class StorageActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/storage.xml");
    ~StorageActivity() override;

    void onContentAvailable() override;

    static void open();

  private:
    int  listener = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    aurore::Box* content = nullptr;

    void rebuild();
    aurore::DetailCell* game_cell(const insights::Played& p);

    AURORE_BIND(aurore::Box, root, "st_root");
};
