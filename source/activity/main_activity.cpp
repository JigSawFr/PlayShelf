// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/main_activity.hpp"

#include "action/download_flow.hpp"
#include "action/insights.hpp"
#include "action/notices.hpp"
#include "action/nlib_flow.hpp"
#include "action/popular_flow.hpp"
#include "action/rollback_flow.hpp"
#include "action/titledb_flow.hpp"
#include <aurore/cadran/ui/cadran_frame.hpp>
#include <aurore/cadran/view/tab_badge.hpp>

#include "action/support_flow.hpp"
#include "action/update_flow.hpp"
#include "action/wishlist.hpp"
#include "activity/insight_notices.hpp"
#include "activity/notices_activity.hpp"
#include "activity/tabs.hpp"
#include "tab/tab_base.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace aurore::literals;

aurore::View* MainActivity::createContentView()
{
    return aurore::View::createFromXMLResource("activity/main.xml");
}

void MainActivity::onContentAvailable()
{
    // For the desktop smoke test: the start screens lead here.
    aurore::Logger::info("main screen opened");
    titledb_flow::start();
    nlib_flow::start();
    popular_flow::start();
    wishlist::start();
    download_flow::init();
    this->update_title();
    // The frame after the design boards.
    cadran::style_frame(dynamic_cast<aurore::AppletFrame*>(this->getContentView()), this->getView("main_tabs"));

    // B on the sidebar quits (the tabs themselves send B back to the sidebar),
    // but only when pressed twice within 2 s: one stray B never closes the app.
    this->getContentView()->registerAction("hints/exit"_i18n, aurore::BUTTON_B, [](aurore::View*) {
        static aurore::Time last_press = 0;
        const aurore::Time now = aurore::getCPUTimeUsec();
        if (last_press && now - last_press < 2000000) {
            last_press = 0;
            aurore::Application::quit();
            return true;
        }
        last_press = now;
        ui::notify("playshelf/hints/exit_again"_i18n);
        return true;
    });

    // Y: the notification bell's list, from any tab, its count in the
    // footer. A tab with its own Ⓨ shows that one instead (the focused
    // view's wins).
    this->getContentView()->registerAction("playshelf/notices/hint"_i18n, aurore::BUTTON_Y, [](aurore::View*) {
        NoticesActivity::open();
        return true;
    });
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    // The desktop build: the keyboard has no Ⓨ, so the Y key (smoke test). Not in
    // a browser, where the page presses Ⓨ itself for the Y key.
    this->getContentView()->registerAction(aurore::AuroreKeyCombination(aurore::AURORE_KBD_KEY_Y), [](aurore::View*) {
        NoticesActivity::open();
        return true;
    });
#endif
    this->notice_listener = notices::listen([this]() { this->update_notice_hint(); });
    this->update_notice_hint();

    // What PlayShelf makes of the console and its servers: the bell's
    // entries, and Home's entry of the sidebar counting the games with
    // something to get.
    this->insights_listener = insights::listen([this]() { this->update_insights(); });
    insights::start();

    // −: the language, the theme and the text size (also a tap on them in
    // the header, which shows them). Left out of the footer first when a
    // screen's own keys need its room (Aurore, patch 0020).
    aurore::setHintsOptionalOwner(this->getContentView());
    this->getContentView()->registerAction("playshelf/quick/hint"_i18n, aurore::BUTTON_BACK, [](aurore::View*) {
        ui::quick_settings(TabBase::refresh_shown);
        return true;
    });
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    // The desktop build: the keyboard has no −, so the minus key (smoke test).
    this->getContentView()->registerAction(aurore::AuroreKeyCombination(aurore::AURORE_KBD_KEY_MINUS), [](aurore::View*) {
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
        aurore::sync([this, start]() {
            auto* frame = dynamic_cast<aurore::TabFrame*>(this->getView("main_tabs"));
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
    notices::unlisten(this->notice_listener);
    insights::unlisten(this->insights_listener);
    if (aurore::getHintsOptionalOwner() == this->getContentView()) aurore::setHintsOptionalOwner(nullptr);
}

void MainActivity::update_notice_hint()
{
    const int n = notices::action_count();
    this->getContentView()->updateActionHint(aurore::BUTTON_Y, n > 0 ? aurore::getStr("playshelf/notices/hint_count", n)
                                                                    : "playshelf/notices/hint"_i18n);
    aurore::Application::getGlobalHintsUpdateEvent()->fire();
}

void MainActivity::update_insights()
{
    const auto snap = insights::now();
    insight_notices::publish(*snap);
    auto* frame   = this->getView("main_tabs");
    auto* sidebar = frame ? dynamic_cast<aurore::Sidebar*>(frame->getView("aurore/tab_frame/sidebar")) : nullptr;
    if (!sidebar) return;
    if (TabBadge* badge = TabBadge::of(sidebar->getItem(tabs::home))) badge->set_count((int)snap->to_get_games);
}

void MainActivity::open_tab(int position, const std::string& row)
{
    auto* frame = dynamic_cast<aurore::TabFrame*>(this->getView("main_tabs"));
    if (!frame) return;
    aurore::sync([frame, position, row]() {
        frame->focusTab(position);   // replaces the tab content
        const auto& children = frame->getChildren();
        if (children.size() < 2) return;
        aurore::View* focus = children.back();
        if (aurore::View* cell = row.empty() ? nullptr : focus->getView(row))
            if (cell->getVisibility() == aurore::Visibility::VISIBLE) focus = cell;
        aurore::Application::giveFocus(focus);
    });
}

void MainActivity::update_title()
{
    auto* frame = dynamic_cast<aurore::AppletFrame*>(this->getContentView());
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
