// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/about_tab.hpp"

#include "action/download_flow.hpp"
#include "action/update_flow.hpp"
#include <borealis/cadran/ui/cadran_frame.hpp>
#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/view/cadran_digits.hpp>

#include "activity/changelog_activity.hpp"
#include "app.hpp"
#include "tab/help_tab.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/update.hpp"
#include "view/funding.hpp"

using namespace brls::literals;

AboutTab::AboutTab()
    : TabBase("xml/tab/about.xml", false), list(this)
{
    namespace kit = cadran::kit;
    support_note->setSingleLine(false);
    // The changes of the running version (the bundled CHANGELOG.md). Seven
    // presses of a hidden Ⓨ turn the developer tools on.
    version->registerAction("playshelf/about/changelog_hint"_i18n, brls::BUTTON_A, [](brls::View*) {
        brls::Application::pushActivity(new ChangelogActivity());
        return true;
    }, false, false, brls::SOUND_CLICK);
    version->registerAction("", brls::BUTTON_Y, [this](brls::View*) {
        this->count_version_press();
        return true;
    }, true, false, brls::SOUND_CLICK);
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    // The desktop build: the keyboard has no Ⓨ, so the Y key (smoke test). Not in
    // a browser, where the page presses Ⓨ itself for the Y key (web/README.md).
    version->registerAction(brls::BrlsKeyCombination(brls::BRLS_KBD_KEY_Y), [this](brls::View*) {
        this->count_version_press();
        return true;
    });
#endif

    update_cell->registerClickAction([](brls::View*) {
        update_flow::check_now();
        return true;
    });

    help->setDetailText(ui::opens());
    help->registerClickAction([](brls::View*) {
        ui::open_screen(new HelpTab(), "playshelf/prefs/help"_i18n);
        return true;
    });

    // Who made it, what it builds on, its licence, and its source as a QR
    // code: one dialog (they were six lines). The funding links do not
    // change either: set once.
    credits->setDetailText(ui::opens("GPLv3"));
    credits->registerClickAction([](brls::View*) {
        const auto line = [](const char* title, const std::string& value) {
            return brls::getStr("playshelf/common/line", brls::getStr(title), value);
        };
        const std::string text = line("playshelf/about/credit_author", "JigSawFr") + "\n" +
                                 line("playshelf/about/credit_base", "PlayGuard") + "\n" +
                                 line("playshelf/about/credit_ui", "borealis (Apache 2.0)") + "\n" +
                                 line("playshelf/about/credit_family", "Boréale (boreale.dev)") + "\n" +
                                 line("playshelf/tools/license", "GPLv3") + "\n" +
                                 line("playshelf/tools/source", app::repo_url());
        ui::qr_dialog(text, app::repo_url());
        return true;
    });
    funding->addView(funding::cards(180, true));
    // The mark, the name and the version first.
    if (brls::Box* column = this->list.content()) {
        brls::Box* hero = kit::top_card(false);
        hero->setAxis(brls::Axis::ROW);
        hero->setAlignItems(brls::AlignItems::CENTER);
        hero->setMarginTop(10);
        hero->setMarginBottom(6);
        hero->setPadding(24, 30, 24, 30);
        auto* badge = new brls::Box(brls::Axis::ROW);
        badge->setWidth(84);
        badge->setHeight(84);
        badge->setCornerRadius(24);
        badge->setShrink(0);
        badge->setThemeColor("backgroundColor", "cadran/teal_soft");
        badge->setAlignItems(brls::AlignItems::CENTER);
        badge->setJustifyContent(brls::JustifyContent::CENTER);
        badge->setMarginRight(22);
        badge->addView(kit::icon("mark", 52, "cadran/teal"));
        hero->addView(badge);
        auto* text = new brls::Box(brls::Axis::COLUMN);
        text->setGrow(1.0f);
        text->setWidth(0);
        auto* name = new CadranDigits();
        name->setText("PlayShelf");
        name->setFontSize(36);
        text->addView(name);
        this->version_line = kit::label("", 16, "cadran/text2");
        this->version_line->setMarginTop(4);
        text->addView(this->version_line);
        brls::Label* tagline = kit::label("playshelf/cadran/about/tagline"_i18n, 15, "cadran/text3");
        tagline->setMarginTop(6);
        text->addView(tagline);
        hero->addView(text);
        column->addView(hero, 0);
    }
    this->list.apply({ { "ab_version", "info" }, { "ab_update", "download" }, { "ab_help", "tools" },
                       { "ab_credits", "user" } });
}

void AboutTab::count_version_press()
{
    if (app::dev_mode()) {
        ui::notify("playshelf/dev/already_on"_i18n);
        return;
    }
    // Seven presses, at most 3 s apart.
    const brls::Time now = brls::getCPUTimeUsec();
    if (now - this->last_version_press > 3000000) this->version_presses = 0;
    this->last_version_press = now;
    const int left = 7 - ++this->version_presses;
    if (left > 0) {
        if (left == 1) ui::notify("playshelf/dev/presses_left_one"_i18n);
        else if (left <= 3) ui::notify(brls::getStr("playshelf/dev/presses_left", left));
        return;
    }
    this->version_presses = 0;
    app::set_dev_mode(true, true, "playshelf/dev/enabled"_i18n,
                      "playshelf/dev/enabled"_i18n + " " + "playshelf/toast/config_err"_i18n);
    ui::on_mode_changed();
}

void AboutTab::refresh()
{
    const auto& cfg = config::get();
    update_cell->setDetailText(cfg.update_checked.empty() ? "" : brls::getStr("playshelf/tools/update_last", cfg.update_checked));
    std::string flags;
    if (app::read_only()) flags += " · " + "playshelf/tools/flag_read_only"_i18n;
    if (app::dev_mode()) flags += " · " + "playshelf/tools/flag_dev"_i18n;
    // Developer mode: the commit too, to tell development builds apart.
    const std::string commit = app::dev_mode() && !app::commit().empty() ? " (" + app::commit() + ")" : "";
    // With the newer release the last update check found.
    const bool newer = !cfg.update_latest.empty() && update::compare_versions(cfg.update_latest, app::version()) > 0;
    const std::string status = newer ? brls::getStr("playshelf/about/version_newer", app::version(), cfg.update_latest)
                                     : app::version();
    version->setDetailText(ui::opens(status + commit + flags));
    version->detail->setThemeColor("textColor", newer ? "cadran/amber" : "cadran/text2");
    this->list.sync();
    if (this->version_line) {
        const std::string version_text = app::commit().empty() ? app::version() : app::version() + " · " + app::commit();
        this->version_line->setText(brls::getStr("playshelf/cadran/about/version", version_text));
    }
    download_flow::show_pill();
}

brls::View* AboutTab::create()
{
    return new AboutTab();
}
