// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/screen_hooks.hpp"

#include <borealis.hpp>

#include <string>

#include <borealis/cadran/ui/hooks.hpp>
#include <borealis/cadran/ui/prefs.hpp>

#include "action/frame_stats_flow.hpp"
#include "action/upload_flow.hpp"
#include "activity/main_activity.hpp"
#include "activity/tabs.hpp"
#include "app.hpp"
#include "core/sim_env.h"
#include "tab/help_tab.hpp"
#include "tab/tab_base.hpp"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

namespace
{
// PlayShelf's logo (branding/mark.svg), Cadran's mark in the header: a play
// button inside the daily play-time ring (75 %).
void mark(NVGcontext* vg, float w, float h)
{
    nvgScale(vg, w / 256.0f, h / 256.0f);
    nvgBeginPath(vg);
    nvgCircle(vg, 128, 128, 104);
    nvgFillColor(vg, nvgRGB(0x0B, 0x14, 0x24));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, 128, 128, 74);
    nvgStrokeWidth(vg, 20);
    nvgStrokeColor(vg, nvgRGB(0x1F, 0x33, 0x55));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgArc(vg, 128, 128, 74, -NVG_PI / 2.0f, -NVG_PI / 2.0f + 0.75f * 2.0f * NVG_PI, NVG_CW);
    nvgLineCap(vg, NVG_ROUND);
    nvgStrokeColor(vg, nvgRGB(0x2E, 0xC4, 0xA6));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 110, 98);
    nvgLineTo(vg, 162, 128);
    nvgLineTo(vg, 110, 158);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(0xE8, 0xEE, 0xF7));
    nvgFill(vg);
    nvgStrokeWidth(vg, 10);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStrokeColor(vg, nvgRGB(0xE8, 0xEE, 0xF7));
    nvgStroke(vg);
}
}   // namespace

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
    h.rail_icons = []() { return std::vector<std::string>{ "shelf", "server", "download", "", "settings", "info" }; };

    h.send_report = []() { upload_flow::choose(); };

    // PlayShelf itself, for the header: its version and commit, its logo,
    // its read-only mode, the console's clock, a tab's name.
    h.version = []() { return app::version(); };
    h.commit = []() { return app::commit(); };
    h.is_release = []() { return app::is_release(); };
    h.read_only = []() { return app::read_only(); };
    h.today = []() { return ui::today_date(); };
    h.mark = mark;
    h.tab_title = [](const std::string& screen) {
        return screen.rfind("tab/", 0) == 0 ? brls::getStr("playshelf/tabs/" + screen.substr(4)) : std::string();
    };
    // The look's preferences live in config.json; the simulators' flags are
    // PLAYSHELF_SIM_* (core/sim_env.h).
    h.save_prefs = []() { return ui::save_config(); };
    h.env_flag = [](const char* name) { return sim_env_flag((std::string("PLAYSHELF_SIM_") + name).c_str()); };
    ui::use_prefs(config::get().look);

    // What the kit's own flows ask (ui/app_hooks.hpp).
    ui::app_hooks::Hooks& a = ui::app_hooks::get();
    a.make_help_tab = []() -> brls::View* { return new HelpTab(); };
    a.mode_changed = []() { frame_stats_flow::apply(); };
}
