// DownloadsTab — the downloads (action/download_flow), a tab of the rail
// so the queue is a press away whatever else is open; the queue itself
// runs whichever screen is shown. The queue
// by game, a card each (download::groups: a base game, its updates and its
// add-ons from one server), its icon (from the Library's cache), the
// game's banner from Nlib API behind its right half, its name,
// where it stands (the package being installed with its percent, speed and
// time left; waiting, paused, installed, why one failed) and a timeline of
// its packages (view/timeline). A on a card: the game's packages, each
// with its state (A on one: pause or resume it, try a failed one again,
// install one refused for a content not signed by Nintendo anyway, remove
// it, what it installed so far deleted), and the game's own pause, resume
// and remove. Ⓧ pauses or resumes them all, Ⓨ clears the
// installed ones. Last, whether the console stays awake while they run
// (config keep_awake), how many connections a download takes at once
// (config connections, install/split_stream) and how many packages are
// fetched ahead while one installs (config prefetch). Updated as the queue changes, the running job's line
// twice a second.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <vector>

#include <aurore/cadran/ui/cadran_kit.hpp>

#include "tab/tab_base.hpp"
#include "view/timeline.hpp"

class BannerBackdrop;

class DownloadsTab : public TabBase
{
  public:
    DownloadsTab();
    ~DownloadsTab() override;
    static aurore::View* create();

  protected:
    void refresh() override;

  private:
    struct Row
    {
        std::vector<std::string> ids;   // its jobs
        aurore::Box*               card = nullptr;
        aurore::Image*             icon = nullptr;
        aurore::Label*             name = nullptr;
        aurore::Label*             state = nullptr;
        Timeline*                timeline = nullptr;
        BannerBackdrop*          backdrop = nullptr;   // the game's banner behind the card
    };

    // The cards made again when the games or their jobs changed (added,
    // removed), else their lines set in place (the focus stays where it is).
    void update();
    void fill(Row& row);
    void open_game(const std::vector<std::string>& ids);
    void open_job(const std::string& job_id);
    void set_hints();

    std::vector<Row> rows;
    int              generation = 0;   // the cards' build: an icon arriving for an older one is freed
    int              listener = 0;

    AURORE_BIND(aurore::Box, empty_box, "dl_empty");
    AURORE_BIND(aurore::Box, list,      "dl_list");
    AURORE_BIND(aurore::Box, settings,  "dl_settings");
    aurore::BooleanCell* keep_awake = nullptr;
    aurore::DetailCell*  connections = nullptr;
    aurore::DetailCell*  prefetch = nullptr;
};
