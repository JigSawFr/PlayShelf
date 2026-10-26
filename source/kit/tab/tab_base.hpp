// TabBase — common shape of every sidebar tab: inflate the XML, re-read the
// state each time the tab (re)appears, X refreshes on demand (on the tabs that
// read the console; the footer shows only what a tab can do), optional periodic
// refresh, and the read-only note (a Label with id "tab_ro_note", if present).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

#include <borealis/cadran/ui/guarded.hpp>


class TabBase : public brls::Box
{
  public:
    // `refreshes`: X reads the tab again. Off for a tab that shows only
    // PlayShelf's own settings and files, which change only from it.
    explicit TabBase(const std::string& xml_res, bool refreshes = true);
    ~TabBase() override;

    // Called when the tab is shown and when the main screen comes back on top
    // (after a screen, a dropdown, a dialog …).
    void willAppear(bool resetState = false) override;

    // Re-reads the tab on screen, if any (after the read-only or developer
    // mode changed).
    static void refresh_shown();
    // The shown tab's screen ("tab/library"), "" for none.
    static std::string shown_screen();
    // The main screen's tab last shown ("tab/about"): not a tab's view on a
    // screen of its own (Help). A sub-screen's way back.
    static std::string main_screen();

    void        onChildFocusGained(brls::View* directChild, brls::View* focusedView) override;

  protected:
    virtual void refresh() = 0;

    // Goes with the tab: what finishes later (a flow's callback after the
    // network or a dialog, a background task's brls::sync) checks it, by
    // ui::guarded, as a tab replaced by another is freed meanwhile.
    ui::Alive alive = ui::make_alive();
    // refresh(), once a flow is over: nothing if the tab is gone by then.
    std::function<void()> refresher() { return ui::guarded(this->alive, [this]() { this->refresh(); }); }

    // This tab is the one on screen. A tab replaced by another is freed a
    // little later (borealis' free queue): until then, its timer and its
    // listeners must not refresh it, or what it sets for the whole screen
    // (the header's pill) would cover the new tab's.
    bool shown() const;

    // Calls refresh() every `period_ms` while the app is in the foreground,
    // this tab is the one shown and its screen is the top one (not under a
    // dialog or another screen). Tabs are deleted when another one is
    // opened, which stops it.
    void enable_auto_refresh(int period_ms);

  private:
    brls::RepeatingTimer auto_timer;
    std::string screen;   // "tab/overview": how the frame statistics name this tab

    bool              focused_once    = false;
    bool              revealed        = false;   // risen into place once (ui/motion)

    void update_ro_note();
};
