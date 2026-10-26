// DownloadsTab — the downloads (action/download_flow), a tab of the rail
// so the queue is a press away whatever else is open; the queue itself
// runs whichever screen is shown. The queue
// by game, a card each (download::groups: a base game, its updates and its
// add-ons from one server), its icon (from the Library's cache), its name,
// where it stands (the package being installed with its percent, speed and
// time left; waiting, paused, installed, why one failed) and a timeline of
// its packages (view/timeline). A on a card: the game's packages, each
// with its state (A on one: pause or resume it, try a failed one again,
// install one refused for a content not signed by Nintendo anyway, remove
// it, what it installed so far deleted), and the game's own pause, resume
// and remove. Ⓧ pauses or resumes them all, Ⓨ clears the
// installed ones. Last, whether the console stays awake while they run
// (config keep_awake) and how many connections a download takes at once
// (config connections, install/split_stream). Updated as the queue changes, the running job's line
// twice a second.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <vector>

#include <borealis/cadran/ui/cadran_kit.hpp>

#include "tab/tab_base.hpp"
#include "view/timeline.hpp"

class DownloadsTab : public TabBase
{
  public:
    DownloadsTab();
    ~DownloadsTab() override;
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    struct Row
    {
        std::vector<std::string> ids;   // its jobs
        brls::Box*               card = nullptr;
        brls::Image*             icon = nullptr;
        brls::Label*             name = nullptr;
        brls::Label*             state = nullptr;
        Timeline*                timeline = nullptr;
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

    BRLS_BIND(brls::Box, empty_box, "dl_empty");
    BRLS_BIND(brls::Box, list,      "dl_list");
    BRLS_BIND(brls::Box, settings,  "dl_settings");
    brls::BooleanCell* keep_awake = nullptr;
    brls::DetailCell*  connections = nullptr;
};
