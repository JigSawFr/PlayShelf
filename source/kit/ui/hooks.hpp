// ui::hooks — what the kit (source/kit) asks of the app above it (the main
// screen, the tabs, a few screens and flows) without including it: the app
// fills this in once at start-up (activity/screen_hooks.cpp), before any
// screen opens. Each hook left empty does nothing (or answers nothing).
// tools/check_sources.py fails on an include of the app's own files from
// source/kit (but app.hpp and util/config.hpp, the app's identity and
// settings).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

namespace ui::hooks
{

struct Hooks
{
    // The main screen (activity/main_activity).
    std::function<bool(brls::Activity* activity)> is_main;   // `activity` is it
    std::function<void(brls::Activity* main, int position, const std::string& row)> open_tab;
    std::function<void(brls::Activity* main)> update_title;
    // A new main screen, pushed, on tab `position`, the focus on `row`.
    std::function<void(int position, const std::string& row)> push_main;

    // The tabs (tab/tab_base): TabBase's statics, and whether a view is a tab.
    std::function<void()>                 refresh_shown;
    std::function<std::string()>          shown_screen;
    std::function<std::string()>          main_screen;
    std::function<bool(brls::View* view)> is_tab;

    // The sidebar position of the tab whose screen name is `screen`
    // ("tab/about"), -1 when there is none; and the position to fall back on.
    std::function<int(const std::string& screen)> tab_position;
    std::function<int()>                          settings_tab;
    // The rail's icons (view/cadran_icon), one per sidebar position, "" for
    // a separator.
    std::function<std::vector<std::string>()>    rail_icons;

    // Screens and flows.
    std::function<brls::View*()>      make_help_tab;          // Help & diagnostics, for a screen of its own
    std::function<void()>             send_report;            // upload_flow::choose
    std::function<void(const char* what, bool ok)> note_write;   // a write that failed, for the app to say
    std::function<void()>             mode_changed;           // what runs only in developer mode

    // Before replacing the running .nro (an update, another build, going
    // back): false stops it, the app having said why (a PIN refused).
    // Unset: allowed.
    std::function<bool()> may_replace_self;

    // News for the app's notifications, when it has some: `id` names the
    // entry (set again: replaced), `action` is its button (`button` its
    // label; nullptr: none). Unset: nothing is kept.
    std::function<void(const std::string& id, const std::string& title, const std::string& text,
                       const std::string& button, std::function<void()> action)>
                                                   notice_set;
    std::function<void(const std::string& id)> notice_clear;
};

// The one set, filled in at start-up.
Hooks& get();

}   // namespace ui::hooks
