// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/main_activity.hpp"

#include "action/download_flow.hpp"
#include "action/rollback_flow.hpp"
#include "action/support_flow.hpp"
#include "action/update_flow.hpp"
#include "activity/tabs.hpp"
#include "tab/tab_base.hpp"
#include "ui/cadran_frame.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

brls::View* MainActivity::createContentView()
{
    return brls::View::createFromXMLResource("activity/main.xml");
}

void MainActivity::onContentAvailable()
{
    // For the desktop smoke test: the start screens lead here.
    brls::Logger::info("main screen opened");
    download_flow::init();
    this->update_title();
    // The frame after the design boards.
    cadran::style_frame(dynamic_cast<brls::AppletFrame*>(this->getContentView()), this->getView("main_tabs"));

    // B on the sidebar quits (the tabs themselves send B back to the sidebar),
    // but only when pressed twice within 2 s: one stray B never closes the app.
    this->getContentView()->registerAction("hints/exit"_i18n, brls::BUTTON_B, [](brls::View*) {
        static brls::Time last_press = 0;
        const brls::Time now = brls::getCPUTimeUsec();
        if (last_press && now - last_press < 2000000) {
            last_press = 0;
            brls::Application::quit();
            return true;
        }
        last_press = now;
        ui::notify("playshelf/hints/exit_again"_i18n);
        return true;
    });

    // −: the language, the theme and the text size (also a tap on them in
    // the header, which shows them). Left out of the footer first when a
    // screen's own keys need its room (Aurore, patch 0020).
    brls::setHintsOptionalOwner(this->getContentView());
    this->getContentView()->registerAction("playshelf/quick/hint"_i18n, brls::BUTTON_BACK, [](brls::View*) {
        ui::quick_settings(TabBase::refresh_shown);
        return true;
    });
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    // The desktop build: the keyboard has no −, so the minus key (smoke test).
    this->getContentView()->registerAction(brls::BrlsKeyCombination(brls::BRLS_KBD_KEY_MINUS), [](brls::View*) {
        ui::quick_settings(TabBase::refresh_shown);
        return true;
    });
#endif

    // Preferences › Start on: that tab rather than the library.
    int start = 0;
    for (const char* t : config::START_TABS) {
        if (config::get().start_tab == t) break;
        start++;
    }
    if (start > 0 && start < (int)(sizeof(config::START_TABS) / sizeof(config::START_TABS[0]))) {
        brls::sync([this, start]() {
            auto* frame = dynamic_cast<brls::TabFrame*>(this->getView("main_tabs"));
            if (frame && ui::on_top(this)) frame->focusTab(tabs::of(start));
        });
    }

    // A newer version (daily when opted in, and while one is known and not
    // installed: its screen); a build gone back from; then, with nothing
    // else in front, "What's new" after an update, else the monthly
    // "Support PlayShelf" reminder.
    update_flow::at_start(this);
    rollback_flow::note_went_back();
    support_flow::at_start(this);
}

MainActivity::~MainActivity()
{
    if (brls::getHintsOptionalOwner() == this->getContentView()) brls::setHintsOptionalOwner(nullptr);
}

void MainActivity::open_tab(int position, const std::string& row)
{
    auto* frame = dynamic_cast<brls::TabFrame*>(this->getView("main_tabs"));
    if (!frame) return;
    brls::sync([frame, position, row]() {
        frame->focusTab(position);   // replaces the tab content
        const auto& children = frame->getChildren();
        if (children.size() < 2) return;
        brls::View* focus = children.back();
        if (brls::View* cell = row.empty() ? nullptr : focus->getView(row))
            if (cell->getVisibility() == brls::Visibility::VISIBLE) focus = cell;
        brls::Application::giveFocus(focus);
    });
}

void MainActivity::update_title()
{
    auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
    if (!frame) return;
    frame->setTitle("playshelf/title"_i18n);
    // The header's pill says when the app is read-only, on every screen.
    cadran::refresh_pill();
}

// Back from a dialog or a screen: a tab may have hidden the view that opened
// it meanwhile.
void MainActivity::onResume()
{
    ui::keep_focus(this);
}
