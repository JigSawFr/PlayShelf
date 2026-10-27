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
#include "util/paths.hpp"
#include "util/config.hpp"

namespace
{
// PlayShelf's logo (branding/mark.svg), Cadran's mark in the header: the
// icon's shelf, its frame and its two shelves in the aurora's colours
// (Aurore's teal to violet) around the night, the games standing on them.
void mark(NVGcontext* vg, float w, float h)
{
    nvgScale(vg, w / 256.0f, h / 256.0f);
    const NVGpaint aurora = nvgLinearGradient(vg, 42, 40, 214, 216, nvgRGB(0x00, 0xFF, 0xCC), nvgRGB(0x8A, 0x5C, 0xFF));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 42, 40, 172, 176, 16);
    nvgFillColor(vg, nvgRGB(0x0E, 0x1A, 0x33));
    nvgFill(vg);
    nvgStrokeWidth(vg, 16);
    nvgStrokePaint(vg, aurora);
    nvgStroke(vg);
    nvgLineCap(vg, NVG_ROUND);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 68, 112);
    nvgLineTo(vg, 188, 112);
    nvgMoveTo(vg, 68, 176);
    nvgLineTo(vg, 188, 176);
    nvgStrokeWidth(vg, 14);
    nvgStrokePaint(vg, aurora);
    nvgStroke(vg);
    // The games: M76 98 L76 72, M100 98 L100 82, M150 98 L150 68, M86 162 L86 132, M134 162 L134 140, M160 162 L160 130.
    static const float GAMES[6][3] = { { 76, 72, 98 }, { 100, 82, 98 }, { 150, 68, 98 },
                                       { 86, 132, 162 }, { 134, 140, 162 }, { 160, 130, 162 } };
    nvgBeginPath(vg);
    for (const auto& g : GAMES) {
        nvgMoveTo(vg, g[0], g[1]);
        nvgLineTo(vg, g[0], g[2]);
    }
    nvgStrokeWidth(vg, 16);
    nvgStrokeColor(vg, nvgRGB(0xE8, 0xEE, 0xF7));
    nvgStroke(vg);
    nvgLineCap(vg, NVG_BUTT);
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
    h.rail_icons = []() { return std::vector<std::string>{ "home", "shelf", "server", "download", "", "settings", "info" }; };

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
    // The user's own theme files, after Aurore's presets in the palette
    // picker (docs/themes.md): sd:/config/playshelf/themes.
    h.themes_dir = []() { return paths::themes_dir(); };
    h.env_flag = [](const char* name) { return sim_env_flag((std::string("PLAYSHELF_SIM_") + name).c_str()); };
    ui::use_prefs(config::get().look);

    // What the kit's own flows ask (ui/app_hooks.hpp).
    ui::app_hooks::Hooks& a = ui::app_hooks::get();
    a.make_help_tab = []() -> brls::View* { return new HelpTab(); };
    a.mode_changed = []() { frame_stats_flow::apply(); };
}
