// ui::app_hooks — what the kit's flows (source/kit/action) ask of the app
// above them, besides what Aurore's Cadran asks (ui::hooks): the app fills
// this in once at start-up (activity/screen_hooks.cpp). Each hook left empty
// does nothing (or answers nothing).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>

namespace ui::app_hooks
{

struct Hooks
{
    std::function<brls::View*()>      make_help_tab;          // Help & diagnostics, for a screen of its own
    std::function<void(const char* what, bool ok)> note_write;   // a write that failed, for the app to say
    std::function<void()>             mode_changed;           // what runs only in developer mode

    // Before replacing the running .nro (an update, another build, going
    // back): false stops it, the app having said why. Unset: allowed.
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

}   // namespace ui::app_hooks
