// HelpTab — About › Help & diagnostics, a screen of its own: the diagnostic
// report (export, send online, and the GitHub account it is sent with, shown
// once signed in), how PlayShelf was launched and its data folder, and how
// many reports and crash files are kept, then the developer tools (shown
// after seven presses on About › Version; among them, clearing the cache
// and the log files, and installing another build).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include "tab/tab_base.hpp"

class HelpTab : public TabBase
{
  public:
    HelpTab();

  protected:
    void refresh() override;

  private:
    aurore::RepeatingTimer stats_timer;   // the frame statistics' last line, every second while they run

    void update_frame_stats();

    AURORE_BIND(aurore::DetailCell,  export_cell, "hp_export");
    AURORE_BIND(aurore::Label,       export_note, "hp_export_note");
    AURORE_BIND(aurore::DetailCell,  upload,      "hp_upload");
    AURORE_BIND(aurore::DetailCell,  github,      "hp_github");
    AURORE_BIND(aurore::DetailCell,  mode,        "hp_mode");
    AURORE_BIND(aurore::DetailCell,  data,        "hp_data");
    AURORE_BIND(aurore::Label,       upload_note, "hp_upload_note");
    AURORE_BIND(aurore::DetailCell,  logs_keep,   "hp_logs_keep");
    AURORE_BIND(aurore::DetailCell,  logs_keep_days, "hp_logs_keep_days");
    AURORE_BIND(aurore::DetailCell,  logs_keep_mb, "hp_logs_keep_mb");
    AURORE_BIND(aurore::Label,       logs_note,   "hp_logs_note");
    AURORE_BIND(aurore::Header,      dev_header,  "hp_dev_header");
    AURORE_BIND(aurore::BooleanCell, dev_mode,    "hp_dev_mode");
    AURORE_BIND(aurore::BooleanCell, dev_read_only, "hp_dev_read_only");
    AURORE_BIND(aurore::BooleanCell, dev_frame_stats, "hp_dev_frame_stats");
    AURORE_BIND(aurore::BooleanCell, dev_glow, "hp_dev_glow");
    AURORE_BIND(aurore::Label,       dev_frame_stats_note, "hp_dev_frame_stats_note");
    AURORE_BIND(aurore::Label,       dev_frame_stats_last, "hp_dev_frame_stats_last");
    AURORE_BIND(aurore::DetailCell,  dev_clear_cache, "hp_dev_clear_cache");
    AURORE_BIND(aurore::DetailCell,  dev_clear_logs, "hp_dev_clear_logs");
    AURORE_BIND(aurore::DetailCell,  dev_report,  "hp_dev_report");
    AURORE_BIND(aurore::DetailCell,  dev_install, "hp_dev_install");
    AURORE_BIND(aurore::DetailCell,  dev_build,   "hp_dev_build");
};
