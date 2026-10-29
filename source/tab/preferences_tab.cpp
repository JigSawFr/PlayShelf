// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/preferences_tab.hpp"

#include <algorithm>
#include <vector>

#include <fmt/format.h>

#include <aurore/cadran/ui/cadran_frame.hpp>

#include "action/download_flow.hpp"
#include "action/titledb_flow.hpp"
#include "action/popular_flow.hpp"
#include "action/update_flow.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace aurore::literals;

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
    return aurore::getStr("playshelf/tabs/" + tab);
}

// A source as the list names it: CyberFoil-DB, else its host and path.
std::string source_name(const std::string& url)
{
    if (url == config::CYBERFOIL_TITLE_SOURCE) return "playshelf/titledb/cyberfoil"_i18n;
    if (url == config::NXMETA_TITLE_SOURCE) return "playshelf/titledb/nxmeta"_i18n;
    if (url == config::GHOSTLAND_TITLE_SOURCE) return "playshelf/titledb/ghostland"_i18n;
    std::string s = url.substr(url.find("://") + 3);
    const size_t last = s.rfind('/');
    if (last != std::string::npos && last > 0) s = s.substr(0, last);
    return s;
}

// "63,880 titles · 20261013" for the sources open, in the config's order.
std::string state_line(const titledb::State& st)
{
    // A title list has no version of its own, and no SHA-256 to check.
    if (st.list) return aurore::getStr("playshelf/titledb/source_line_list", source_name(st.url), st.count);
    return aurore::getStr("playshelf/titledb/source_line", source_name(st.url), st.version, st.count);
}

size_t titles_known()
{
    size_t n = 0;
    for (const titledb::State& st : titledb_flow::status().sources) n = std::max(n, st.count);
    return n;
}

void save_sources()
{
    ui::save_config();
    titledb_flow::sources_changed();
}
}   // namespace

PreferencesTab::PreferencesTab()
    : TabBase("xml/tab/preferences.xml", false), list(this)
{
    language->registerClickAction([this](aurore::View*) {
        ui::pick_language(this->refresher());
        return true;
    });

    theme->registerClickAction([this](aurore::View*) {
        ui::pick_theme(this->refresher());
        return true;
    });
    // The theme's colours: PlayShelf's own, Aurore's presets or the user's
    // files (sd:/config/playshelf/themes, docs/themes.md), switched at once.
    palette->registerClickAction([this](aurore::View*) {
        ui::pick_palette(this->refresher());
        return true;
    });
    text_size->registerClickAction([this](aurore::View*) {
        ui::pick_text_size(this->refresher());
        return true;
    });
    effects->registerClickAction([this](aurore::View*) {
        std::vector<std::string> labels;
        for (const char* e : config::EFFECTS) labels.push_back(aurore::getStr(std::string("playshelf/prefs/effects_values/") + e));
        ui::pick("playshelf/prefs/effects"_i18n, labels, index_of(config::EFFECTS, config::get().look.effects), [this](int i) {
            config::get().look.effects = config::EFFECTS[i];   // the next frame draws by it
            ui::save_config();
            this->refresh();
        });
        return true;
    });

    start_tab->registerClickAction([this](aurore::View*) {
        std::vector<std::string> labels;
        for (const char* t : config::START_TABS) labels.push_back(tab_name(t));
        // Each tab's icon, as the sidebar draws it (ui/cadran_frame.cpp).
        const std::vector<std::string> icons = { "home", "shelf", "server", "settings", "info" };
        static_assert(sizeof(config::START_TABS) / sizeof(config::START_TABS[0]) == 5, "an icon for each tab");
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
    update_via->registerClickAction([this](aurore::View*) {
        update_flow::choose_method(this->refresher());
        return true;
    });

    // The title database: fetched, its sources.
    titledb->registerClickAction([this](aurore::View*) {
        this->titledb_sheet();
        return true;
    });
    title_sources->registerClickAction([this](aurore::View*) {
        this->sources_sheet();
        return true;
    });
    // The icons none of them has and the games' pages, from Nlib API.
    nlib_api->init("playshelf/titledb/nlib_api"_i18n, config::get().nlib_api, [](bool on) {
        config::get().nlib_api = on;
        ui::save_config();
    });
    // Home's popular games, from Wikipedia's readers (action/popular_flow).
    popular_online->init("playshelf/titledb/popular_online"_i18n, config::get().popular_online, [](bool on) {
        config::get().popular_online = on;
        ui::save_config();
        if (on) popular_flow::refresh();
    });
    this->titledb_listener = titledb_flow::listen(ui::guarded(this->alive, [this]() { this->show_titledb(); }));

    this->list.tiles({ { "pf_language", "language" }, { "pf_theme", "contrast" }, { "pf_text_size", "eye" },
                       { "pf_start_tab", "home" } }, false);
    this->list.apply({ { "pf_palette", "pencil" }, { "pf_effects", "sparkle" }, { "pf_intro", "bolt" },
                       { "pf_support_reminder", "heart" },
                       { "pf_update_daily", "calendar" }, { "pf_update_via", "server" },
                       { "pf_titledb", "shelf" }, { "pf_title_sources", "list" }, { "pf_nlib_api", "globe" },
                       { "pf_popular_online", "bolt" } });
}

PreferencesTab::~PreferencesTab()
{
    titledb_flow::unlisten(this->titledb_listener);
}

void PreferencesTab::show_titledb()
{
    const titledb_flow::Status& st = titledb_flow::status();
    std::string                 line;
    if (st.running)
        line = st.total ? aurore::getStr("playshelf/titledb/fetching", (int)(100 * st.done / st.total)) : "playshelf/titledb/checking"_i18n;
    else if (!st.open) line = "playshelf/titledb/reading"_i18n;
    else if (st.sources.empty()) line = "playshelf/titledb/none"_i18n;
    else line = aurore::getStr("playshelf/titledb/count", titles_known());
    titledb->setDetailText(line);
    const auto& sources = config::get().title_sources;
    title_sources->setDetailText(sources.empty()       ? "playshelf/titledb/no_sources"_i18n
                                 : sources.size() == 1 ? source_name(sources.front())
                                                       : aurore::getStr("playshelf/titledb/sources_count", sources.size()));
}

void PreferencesTab::titledb_sheet()
{
    using Choice = ui::Choice;
    const titledb_flow::Status& st = titledb_flow::status();
    std::vector<Choice>         lines;
    if (st.running)
        lines.emplace_back("playshelf/titledb/stop"_i18n, []() { titledb_flow::stop(); }, "stop");
    else if (!config::get().title_sources.empty())
        lines.emplace_back(st.sources.empty() ? "playshelf/titledb/download"_i18n : "playshelf/titledb/update"_i18n, []() {
            if (ui::refuse_read_only()) return;
            titledb_flow::update();
        }, "download");
    lines.emplace_back("playshelf/titledb/sources"_i18n, ui::guarded(this->alive, [this]() { this->sources_sheet(); }), "list");
    std::string text = "playshelf/titledb/info"_i18n;
    for (const titledb::State& s : st.sources) text += "\n" + state_line(s);
    if (!st.error.empty()) text += "\n" + aurore::getStr("playshelf/titledb/failed", st.error);
    ui::pick_actions("playshelf/titledb/row"_i18n, lines, text);
}

void PreferencesTab::sources_sheet()
{
    using Choice = ui::Choice;
    const auto&         sources = config::get().title_sources;
    std::vector<Choice> lines;
    for (size_t i = 0; i < sources.size(); i++)
        lines.emplace_back(source_name(sources[i]), ui::guarded(this->alive, [this, i]() { this->source_sheet(i); }), "server",
                           fmt::format("{}", i + 1));
    if (sources.size() < config::MAX_TITLE_SOURCES)
        lines.emplace_back("playshelf/titledb/add"_i18n, ui::guarded(this->alive, [this]() {
            if (ui::refuse_read_only()) return;
            ui::prompt_text("playshelf/titledb/add_header"_i18n, "https://", 512, ui::guarded(this->alive, [this](const std::string& url) {
                auto& urls = config::get().title_sources;
                if (url.rfind("https://", 0) != 0 || url.size() <= 8) return ui::refuse("playshelf/titledb/not_https"_i18n);
                if (std::find(urls.begin(), urls.end(), url) != urls.end()) return ui::refuse("playshelf/titledb/duplicate"_i18n);
                if (urls.size() >= config::MAX_TITLE_SOURCES) return;
                urls.push_back(url);
                save_sources();
                this->show_titledb();
                ui::notify("playshelf/titledb/added"_i18n, ui::Toast::Done);
            }));
        }), "plus");
    // A default source removed: offered back.
    for (const char* d : config::DEFAULT_TITLE_SOURCES)
        if (std::find(sources.begin(), sources.end(), d) == sources.end() && sources.size() < config::MAX_TITLE_SOURCES)
            lines.emplace_back(aurore::getStr("playshelf/titledb/restore", source_name(d)), ui::guarded(this->alive, [this, d]() {
                if (ui::refuse_read_only()) return;
                auto& urls = config::get().title_sources;
                if (std::find(urls.begin(), urls.end(), d) != urls.end() || urls.size() >= config::MAX_TITLE_SOURCES) return;
                urls.emplace_back(d);
                save_sources();
                this->show_titledb();
            }), "restore");
    ui::pick_actions("playshelf/titledb/sources"_i18n, lines, "playshelf/titledb/sources_info"_i18n);
}

void PreferencesTab::source_sheet(size_t index)
{
    using Choice = ui::Choice;
    const auto& sources = config::get().title_sources;
    if (index >= sources.size()) return;
    const std::string   url = sources[index];
    // After a change: the list again, as it now stands.
    auto moved = [this](std::vector<std::string> next) {
        config::get().title_sources = std::move(next);
        save_sources();
        this->show_titledb();
    };
    std::vector<Choice> lines;
    if (index > 0)
        lines.emplace_back("playshelf/titledb/up"_i18n, ui::guarded(this->alive, [url, moved]() {
            if (ui::refuse_read_only()) return;
            auto       urls = config::get().title_sources;
            const auto it = std::find(urls.begin(), urls.end(), url);
            if (it == urls.end() || it == urls.begin()) return;
            std::iter_swap(it, it - 1);
            moved(urls);
        }), "caret-up");
    if (index + 1 < sources.size())
        lines.emplace_back("playshelf/titledb/down"_i18n, ui::guarded(this->alive, [url, moved]() {
            if (ui::refuse_read_only()) return;
            auto       urls = config::get().title_sources;
            const auto it = std::find(urls.begin(), urls.end(), url);
            if (it == urls.end() || it + 1 == urls.end()) return;
            std::iter_swap(it, it + 1);
            moved(urls);
        }), "caret-down");
    // Asked first: its packs go from the SD card.
    lines.emplace_back("playshelf/titledb/remove"_i18n, ui::guarded(this->alive, [this, url, moved]() {
        if (ui::refuse_read_only()) return;
        ui::confirm_danger(aurore::getStr("playshelf/titledb/remove_body", source_name(url)), "playshelf/titledb/remove"_i18n,
                           ui::guarded(this->alive, [url, moved]() {
                               auto urls = config::get().title_sources;
                               urls.erase(std::remove(urls.begin(), urls.end(), url), urls.end());
                               moved(urls);
                           }));
    }), "trash", "", "", Choice::Tone::DANGER);
    std::string text = url;
    for (const titledb::State& s : titledb_flow::status().sources)
        if (s.url == url) text += "\n" + state_line(s);
    ui::pick_actions(source_name(url), lines, text);
}

void PreferencesTab::refresh()
{
    const auto& cfg = config::get();
    language->setDetailText(ui::language_name(true));
    theme->setDetailText(ui::theme_name(true));
    palette->setDetailText(ui::palette_name());
    text_size->setDetailText(ui::text_size_name(true));
    effects->setDetailText(aurore::getStr("playshelf/prefs/effects_values/" + std::string(config::EFFECTS[index_of(config::EFFECTS, cfg.look.effects)])));
    start_tab->setDetailText(tab_name(config::START_TABS[index_of(config::START_TABS, cfg.start_tab)]));
    intro->setOn(cfg.intro, false);
    support_reminder->setOn(cfg.support_reminder, false);
    update_daily->setOn(cfg.update_daily, false);
    nlib_api->setOn(cfg.nlib_api, false);
    popular_online->setOn(cfg.popular_online, false);
    update_via->setDetailText(update_flow::method_text());
    this->show_titledb();
    this->list.sync();
    download_flow::show_pill();
}

aurore::View* PreferencesTab::create()
{
    return new PreferencesTab();
}
