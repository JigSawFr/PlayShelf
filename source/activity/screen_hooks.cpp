// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/screen_hooks.hpp"

#include <aurore.hpp>

#include <string>

#include <aurore/cadran/ui/hooks.hpp>
#include <aurore/cadran/ui/prefs.hpp>

#include "action/frame_stats_flow.hpp"
#include "action/shared_look.hpp"
#include "action/upload_flow.hpp"
#include "activity/main_activity.hpp"
#include "activity/notices_activity.hpp"
#include "activity/tabs.hpp"
#include "app.hpp"
#include "core/sim_env.h"
#include "tab/help_tab.hpp"
#include "tab/tab_base.hpp"
#include "action/updates_for_all.hpp"
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
// The look PlayShelf shares with the other Boréale homebrews (config
// look_shared): its words, as config::sanitize keeps them.
shared_look::Look look_of(const config::Config& c)
{
    return { c.look.language, c.look.theme, c.look.text_size, c.look.effects };
}

bool look_ok(const std::string& key, const std::string& value)
{
    config::Config c = config::get();
    std::string*   field = key == "language"  ? &c.look.language
                         : key == "theme"     ? &c.look.theme
                         : key == "text_size" ? &c.look.text_size
                         : key == "effects"   ? &c.look.effects
                                              : nullptr;
    if (!field) return false;
    *field = value;
    config::sanitize(c);
    return *field == value;
}

void write_shared(const std::string& text)
{
    std::string       error;
    const std::string file = shared_look::file(paths::sd_root());
    if (text.empty()) return;
    if (!paths::ensure_dir(file.substr(0, file.rfind('/'))) || !paths::atomic_write(file, text, &error))
        aurore::Logger::warning("shared look: {} not written: {}", file, error);
}

// At start: the shared look's values replace PlayShelf's, before the first
// screen is drawn.
void follow_shared_look()
{
    config::Config& c = config::get();
    if (!c.look_shared) return;
    std::string text;
    paths::read_file(shared_look::file(paths::sd_root()), text);
    shared_look::Look look = look_of(c);
    write_shared(shared_look::follow(text, look, look_ok));
    c.look.language = look.language;
    c.look.theme = look.theme;
    c.look.text_size = look.text_size;
    c.look.effects = look.effects;
}

// After a change saved: the others take it at their next start.
void share_look()
{
    const config::Config& c = config::get();
    if (!c.look_shared) return;
    std::string text;
    paths::read_file(shared_look::file(paths::sd_root()), text);
    write_shared(shared_look::publish(text, look_of(c)));
}
}   // namespace

void install_screen_hooks()
{
    ui::hooks::Hooks& h = ui::hooks::get();

    h.is_main = [](aurore::Activity* a) { return dynamic_cast<MainActivity*>(a) != nullptr; };
    h.open_tab = [](aurore::Activity* main, int position, const std::string& row) {
        static_cast<MainActivity*>(main)->open_tab(position, row);
    };
    h.update_title = [](aurore::Activity* main) { static_cast<MainActivity*>(main)->update_title(); };
    h.push_main = [](int position, const std::string& row) {
        auto* main = new MainActivity();
        aurore::Application::pushActivity(main, aurore::TransitionAnimation::NONE);
        main->open_tab(position, row);
    };

    h.refresh_shown = []() { TabBase::refresh_shown(); };
    h.shown_screen = []() { return TabBase::shown_screen(); };
    h.main_screen = []() { return TabBase::main_screen(); };
    h.is_tab = [](aurore::View* v) { return dynamic_cast<TabBase*>(v) != nullptr; };
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
        return screen.starts_with("tab/") ? aurore::getStr("playshelf/tabs/" + screen.substr(4)) : std::string();
    };
    // The look's preferences live in config.json, and in the look the
    // Boréale homebrews share while Preferences says so (action/shared_look);
    // the simulators' flags are PLAYSHELF_SIM_* (core/sim_env.h).
    h.save_prefs = []() {
        const bool saved = ui::save_config();
        share_look();
        return saved;
    };
    // The user's own theme files, after Aurore's presets in the palette
    // picker (docs/themes.md): sd:/config/playshelf/themes.
    h.themes_dir = []() { return paths::themes_dir(); };
    h.env_flag = [](const char* name) { return sim_env_flag((std::string("PLAYSHELF_SIM_") + name).c_str()); };
    follow_shared_look();
    ui::use_prefs(config::get().look);

    // What the kit's own flows ask (ui/app_hooks.hpp).
    ui::app_hooks::Hooks& a = ui::app_hooks::get();
    a.make_help_tab = []() -> aurore::View* { return new HelpTab(); };
    a.mode_changed = []() { frame_stats_flow::apply(); };
    a.open_notices = []() { NoticesActivity::open(); };
    a.update_checked = [](const update::Result& r) { updates_for_all::check(r); };
}
