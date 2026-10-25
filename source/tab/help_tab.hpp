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
    brls::RepeatingTimer stats_timer;   // the frame statistics' last line, every second while they run

    void update_frame_stats();

    BRLS_BIND(brls::DetailCell,  export_cell, "hp_export");
    BRLS_BIND(brls::Label,       export_note, "hp_export_note");
    BRLS_BIND(brls::DetailCell,  upload,      "hp_upload");
    BRLS_BIND(brls::DetailCell,  github,      "hp_github");
    BRLS_BIND(brls::DetailCell,  mode,        "hp_mode");
    BRLS_BIND(brls::DetailCell,  data,        "hp_data");
    BRLS_BIND(brls::Label,       upload_note, "hp_upload_note");
    BRLS_BIND(brls::DetailCell,  logs_keep,   "hp_logs_keep");
    BRLS_BIND(brls::DetailCell,  logs_keep_days, "hp_logs_keep_days");
    BRLS_BIND(brls::DetailCell,  logs_keep_mb, "hp_logs_keep_mb");
    BRLS_BIND(brls::Label,       logs_note,   "hp_logs_note");
    BRLS_BIND(brls::Header,      dev_header,  "hp_dev_header");
    BRLS_BIND(brls::BooleanCell, dev_mode,    "hp_dev_mode");
    BRLS_BIND(brls::BooleanCell, dev_read_only, "hp_dev_read_only");
    BRLS_BIND(brls::BooleanCell, dev_frame_stats, "hp_dev_frame_stats");
    BRLS_BIND(brls::BooleanCell, dev_glow, "hp_dev_glow");
    BRLS_BIND(brls::Label,       dev_frame_stats_note, "hp_dev_frame_stats_note");
    BRLS_BIND(brls::Label,       dev_frame_stats_last, "hp_dev_frame_stats_last");
    BRLS_BIND(brls::DetailCell,  dev_clear_cache, "hp_dev_clear_cache");
    BRLS_BIND(brls::DetailCell,  dev_clear_logs, "hp_dev_clear_logs");
    BRLS_BIND(brls::DetailCell,  dev_report,  "hp_dev_report");
    BRLS_BIND(brls::DetailCell,  dev_install, "hp_dev_install");
    BRLS_BIND(brls::DetailCell,  dev_build,   "hp_dev_build");
};
