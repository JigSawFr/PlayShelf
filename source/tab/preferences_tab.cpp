// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/preferences_tab.hpp"

#include <vector>

#include <borealis/cadran/ui/cadran_frame.hpp>

#include "action/download_flow.hpp"
#include "action/update_flow.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

namespace
{
template <size_t N>
int index_of(const char* const (&list)[N], const std::string& value)
{
    for (size_t i = 0; i < N; i++)
        if (value == list[i]) return (int)i;
    return 0;
}

std::string tab_name(const std::string& tab)
{
    return brls::getStr("playshelf/tabs/" + tab);
}
}   // namespace

PreferencesTab::PreferencesTab()
    : TabBase("xml/tab/preferences.xml", false), list(this)
{
    language->registerClickAction([this](brls::View*) {
        ui::pick_language(this->refresher());
        return true;
    });

    theme->registerClickAction([this](brls::View*) {
        ui::pick_theme(this->refresher());
        return true;
    });
    text_size->registerClickAction([this](brls::View*) {
        ui::pick_text_size(this->refresher());
        return true;
    });
    effects->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const char* e : config::EFFECTS) labels.push_back(brls::getStr(std::string("playshelf/prefs/effects_values/") + e));
        ui::pick("playshelf/prefs/effects"_i18n, labels, index_of(config::EFFECTS, config::get().look.effects), [this](int i) {
            config::get().look.effects = config::EFFECTS[i];   // the next frame draws by it
            ui::save_config();
            this->refresh();
        });
        return true;
    });

    start_tab->registerClickAction([this](brls::View*) {
        std::vector<std::string> labels;
        for (const char* t : config::START_TABS) labels.push_back(tab_name(t));
        // Each tab's icon, as the sidebar draws it (ui/cadran_frame.cpp).
        const std::vector<std::string> icons = { "shelf", "server", "settings", "info" };
        static_assert(sizeof(config::START_TABS) / sizeof(config::START_TABS[0]) == 4, "an icon for each tab");
        ui::pick_grid("playshelf/tools/start_tab"_i18n, labels, icons, index_of(config::START_TABS, config::get().start_tab), [this](int i) {
            config::get().start_tab = config::START_TABS[i];
            ui::save_config();
            this->refresh();
        });
        return true;
    });

    intro->init("playshelf/prefs/intro"_i18n, config::get().intro, [](bool on) {
        config::get().intro = on;
        ui::save_config();
    });
    // Support PlayShelf once a month at start-up (action/support_flow).
    support_reminder->init("playshelf/prefs/support_reminder"_i18n, config::get().support_reminder, [](bool on) {
        config::get().support_reminder = on;
        ui::save_config();
    });

    // How PlayShelf updates (checking now is in About, by the version).
    update_daily->init("playshelf/tools/update_daily"_i18n, config::get().update_daily, [](bool on) {
        config::get().update_daily = on;
        ui::save_config();
    });
    update_via->registerClickAction([this](brls::View*) {
        update_flow::choose_method(this->refresher());
        return true;
    });

    this->list.tiles({ { "pf_language", "language" }, { "pf_theme", "contrast" }, { "pf_text_size", "eye" },
                       { "pf_start_tab", "home" } }, false);
    this->list.apply({ { "pf_effects", "sparkle" }, { "pf_intro", "bolt" }, { "pf_support_reminder", "heart" },
                       { "pf_update_daily", "calendar" }, { "pf_update_via", "server" } });
}

void PreferencesTab::refresh()
{
    const auto& cfg = config::get();
    language->setDetailText(ui::language_name(true));
    theme->setDetailText(ui::theme_name(true));
    text_size->setDetailText(ui::text_size_name(true));
    effects->setDetailText(brls::getStr("playshelf/prefs/effects_values/" + std::string(config::EFFECTS[index_of(config::EFFECTS, cfg.look.effects)])));
    start_tab->setDetailText(tab_name(config::START_TABS[index_of(config::START_TABS, cfg.start_tab)]));
    intro->setOn(cfg.intro, false);
    support_reminder->setOn(cfg.support_reminder, false);
    update_daily->setOn(cfg.update_daily, false);
    update_via->setDetailText(update_flow::method_text());
    this->list.sync();
    download_flow::show_pill();
}

brls::View* PreferencesTab::create()
{
    return new PreferencesTab();
}
