// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/screen_hooks.hpp"

#include <borealis.hpp>

#include "action/frame_stats_flow.hpp"
#include "action/upload_flow.hpp"
#include "activity/main_activity.hpp"
#include "activity/tabs.hpp"
#include "tab/help_tab.hpp"
#include "tab/tab_base.hpp"
#include "ui/hooks.hpp"

void install_screen_hooks()
{
    ui::hooks::Hooks& h = ui::hooks::get();

    h.is_main = [](brls::Activity* a) { return dynamic_cast<MainActivity*>(a) != nullptr; };
    h.open_tab = [](brls::Activity* main, int position, const std::string& row) {
        static_cast<MainActivity*>(main)->open_tab(position, row);
    };
    h.update_title = [](brls::Activity* main) { static_cast<MainActivity*>(main)->update_title(); };
    h.push_main = [](int position, const std::string& row) {
        auto* main = new MainActivity();
        brls::Application::pushActivity(main, brls::TransitionAnimation::NONE);
        main->open_tab(position, row);
    };

    h.refresh_shown = []() { TabBase::refresh_shown(); };
    h.shown_screen = []() { return TabBase::shown_screen(); };
    h.main_screen = []() { return TabBase::main_screen(); };
    h.is_tab = [](brls::View* v) { return dynamic_cast<TabBase*>(v) != nullptr; };
    h.tab_position = [](const std::string& screen) { return tabs::position_of(screen); };
    h.settings_tab = []() { return tabs::preferences; };
    h.rail_icons = []() { return std::vector<std::string>{ "shelf", "", "settings", "info" }; };

    h.make_help_tab = []() -> brls::View* { return new HelpTab(); };
    h.send_report = []() { upload_flow::choose(); };
    h.mode_changed = []() { frame_stats_flow::apply(); };
}
