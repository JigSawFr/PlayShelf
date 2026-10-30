// PreferencesTab — PlayShelf itself: language, theme, text size and start
// tab as tiles, the palette (a preset or a theme file), visual effects and
// the intro; the monthly "Support PlayShelf" reminder; how PlayShelf
// updates; the title database (action/titledb_flow: fetched, its sources in
// their order; the icons it lacks asked of Nlib API or not); Home's
// suggestions put away (action/not_interested), to show again. A setting
// that shapes one feature lives in that feature's tab; checking for
// updates now and Help & diagnostics are in About.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore/cadran/ui/cadran_list.hpp>

#include "tab/tab_base.hpp"

class PreferencesTab : public TabBase
{
  public:
    PreferencesTab();
    ~PreferencesTab() override;
    static aurore::View* create();

  protected:
    void refresh() override;

  private:
    void titledb_sheet();
    void sources_sheet();
    void source_sheet(size_t index);
    void show_titledb();

    cadran::List list;
    int          titledb_listener = 0;
    AURORE_BIND(aurore::DetailCell,  language,      "pf_language");
    AURORE_BIND(aurore::DetailCell,  theme,         "pf_theme");
    AURORE_BIND(aurore::DetailCell,  palette,       "pf_palette");
    AURORE_BIND(aurore::DetailCell,  start_tab,     "pf_start_tab");
    AURORE_BIND(aurore::DetailCell,  text_size,     "pf_text_size");
    AURORE_BIND(aurore::DetailCell,  effects,       "pf_effects");
    AURORE_BIND(aurore::BooleanCell, intro,         "pf_intro");
    AURORE_BIND(aurore::BooleanCell, support_reminder, "pf_support_reminder");
    AURORE_BIND(aurore::BooleanCell, notices_others, "pf_notices_others");
    AURORE_BIND(aurore::BooleanCell, update_daily,   "pf_update_daily");
    AURORE_BIND(aurore::DetailCell,  update_via,     "pf_update_via");
    AURORE_BIND(aurore::DetailCell,  titledb,        "pf_titledb");
    AURORE_BIND(aurore::DetailCell,  title_sources,  "pf_title_sources");
    AURORE_BIND(aurore::BooleanCell, nlib_api,       "pf_nlib_api");
    AURORE_BIND(aurore::BooleanCell, popular_online, "pf_popular_online");
    AURORE_BIND(aurore::DetailCell,  not_interested, "pf_not_interested");
};
