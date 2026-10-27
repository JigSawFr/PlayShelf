// PreferencesTab — PlayShelf itself: language, theme, text size and start
// tab as tiles, the palette (a preset or a theme file), visual effects and
// the intro; the monthly "Support PlayShelf" reminder; how PlayShelf
// updates; the title database (action/titledb_flow: fetched, its sources in
// their order; the icons it lacks asked of Nlib API or not). A setting
// that shapes one feature lives in that feature's tab; checking for
// updates now and Help & diagnostics are in About.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis/cadran/ui/cadran_list.hpp>

#include "tab/tab_base.hpp"

class PreferencesTab : public TabBase
{
  public:
    PreferencesTab();
    ~PreferencesTab() override;
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    void titledb_sheet();
    void sources_sheet();
    void source_sheet(size_t index);
    void show_titledb();

    cadran::List list;
    int          titledb_listener = 0;
    BRLS_BIND(brls::DetailCell,  language,      "pf_language");
    BRLS_BIND(brls::DetailCell,  theme,         "pf_theme");
    BRLS_BIND(brls::DetailCell,  palette,       "pf_palette");
    BRLS_BIND(brls::DetailCell,  start_tab,     "pf_start_tab");
    BRLS_BIND(brls::DetailCell,  text_size,     "pf_text_size");
    BRLS_BIND(brls::DetailCell,  effects,       "pf_effects");
    BRLS_BIND(brls::BooleanCell, intro,         "pf_intro");
    BRLS_BIND(brls::BooleanCell, support_reminder, "pf_support_reminder");
    BRLS_BIND(brls::BooleanCell, update_daily,   "pf_update_daily");
    BRLS_BIND(brls::DetailCell,  update_via,     "pf_update_via");
    BRLS_BIND(brls::DetailCell,  titledb,        "pf_titledb");
    BRLS_BIND(brls::DetailCell,  title_sources,  "pf_title_sources");
    BRLS_BIND(brls::BooleanCell, nlib_api,       "pf_nlib_api");
};
