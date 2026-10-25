// DownloadsActivity — Library › Downloads (action/download_flow): every job
// of the queue as a card, its name, its state (waiting, installing with
// its percent, speed and time left, paused, installed, why it failed) and
// a bar. A on a card: pause or resume it, try a failed one again, install
// one refused for a content not signed by Nintendo anyway, remove it (what
// it installed so far deleted). Ⓧ pauses or resumes them all, Ⓨ clears the
// installed ones. Last, whether the console stays awake while they run
// (config keep_awake). Updated as the queue changes, the running job's line
// twice a second.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <vector>

#include "ui/cadran_kit.hpp"

class DownloadsActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/downloads.xml");

    ~DownloadsActivity() override;
    void onContentAvailable() override;

  private:
    struct Row
    {
        std::string          id;
        brls::Box*           card = nullptr;
        brls::Label*         name = nullptr;
        brls::Label*         state = nullptr;
        cadran::kit::Bar*    bar = nullptr;
    };

    // The cards made again when the jobs changed (added, removed), else
    // their lines set in place (the focus stays where it is).
    void update();
    void fill(Row& row);
    void open_job(const std::string& id);
    void set_hints();

    std::vector<Row> rows;
    int              listener = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    BRLS_BIND(brls::Box, empty_box, "dl_empty");
    BRLS_BIND(brls::Box, list,      "dl_list");
    BRLS_BIND(brls::BooleanCell, keep_awake, "dl_keep_awake");
};
