// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/live_prefs.hpp"

#include <borealis.hpp>
#include <functional>

#include "ui/hooks.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

namespace live_prefs
{
namespace
{
brls::ThemeVariant s_system_theme = brls::ThemeVariant::DARK;
std::string s_system_locale;   // the console's language
std::string s_locale;          // the strings' language now

// The tab shown, as a sidebar position (ui::hooks::tab_position), the
// settings' tab when none is known.
int shown_tab()
{
    const auto& h = ui::hooks::get();
    const std::string screen = h.shown_screen ? h.shown_screen() : "";
    const int position = h.tab_position ? h.tab_position(screen) : -1;
    if (position >= 0) return position;
    return h.settings_tab ? h.settings_tab() : 0;
}

// Every screen above the loading one closed, one at a time; then `next`.
void close_all(std::function<void()> next)
{
    if (!brls::Application::popActivity(brls::TransitionAnimation::NONE, [next]() { close_all(next); })) next();
}

// The focused line of the shown tab: its id (or its nearest parent's that
// has one), "" when the focus is elsewhere (the rail, the header).
std::string focused_row()
{
    const auto& is_tab = ui::hooks::get().is_tab;
    if (!is_tab) return "";
    for (brls::View* v = brls::Application::getCurrentFocus(); v; v = v->getParent()) {
        if (is_tab(v)) return "";
        if (!v->getId().empty()) {
            for (brls::View* up = v->getParent(); up; up = up->getParent())
                if (is_tab(up)) return v->getId();
            return "";
        }
    }
    return "";
}

// Every screen made again, the main one on the tab that was shown, the focus
// back on its line: the screens above it (a sub-screen the header's setting
// was changed from) go.
void rebuild()
{
    brls::sync([]() {
        const int tab = shown_tab();
        const std::string row = focused_row();
        close_all([tab, row]() {
            if (ui::hooks::get().push_main) ui::hooks::get().push_main(tab, row);
        });
    });
}

}   // namespace

void start()
{
    // The platform read the console's language (main.cpp leaves borealis'
    // locale on auto), and the strings are in it: the preferences' instead.
    s_system_theme = brls::Application::getPlatform()->getThemeVariant();
    s_system_locale = brls::Application::getLocale();
    const std::string& chosen = config::get().language;
    s_locale = chosen == "system" ? s_system_locale : chosen;
    if (s_locale != s_system_locale) brls::loadTranslations(s_locale);
}

void apply_theme()
{
    const std::string& theme = config::get().theme;
    brls::Application::getPlatform()->setThemeVariant(theme == "dark"    ? brls::ThemeVariant::DARK
                                                      : theme == "light" ? brls::ThemeVariant::LIGHT
                                                                         : s_system_theme);
}

std::string locale()
{
    return s_locale;
}

void theme_changed()
{
    apply_theme();
    rebuild();
}

void language_changed()
{
    const std::string& chosen = config::get().language;
    s_locale = chosen == "system" ? s_system_locale : chosen;
    brls::loadTranslations(s_locale);
    ui::use_font_for(s_locale);   // Chinese has its own (made again with the screens)
    rebuild();
}
}   // namespace live_prefs
