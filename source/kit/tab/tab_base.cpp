// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/tab_base.hpp"

#include "action/frame_stats_flow.hpp"
#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/motion.hpp>

#include "app.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

// The tab last shown (tabs are deleted when another one is opened).
static TabBase* s_shown = nullptr;
static std::string s_main_screen;   // main_screen()

TabBase::TabBase(const std::string& xml_res, bool refreshes)
{
    this->inflateFromXMLRes(xml_res);
    // "xml/tab/overview.xml" -> "tab/overview"
    this->screen = xml_res.substr(xml_res.rfind("xml/") == 0 ? 4 : 0);
    if (this->screen.size() > 4 && this->screen.compare(this->screen.size() - 4, 4, ".xml") == 0)
        this->screen.resize(this->screen.size() - 4);
    if (refreshes)
        this->registerAction("playshelf/hints/refresh"_i18n, aurore::BUTTON_X, [this](aurore::View*) {
            this->refresh();
            return true;
        });
    if (auto* note = dynamic_cast<aurore::Label*>(this->getView("tab_ro_note"))) note->setSingleLine(false);
}

TabBase::~TabBase()
{
    if (s_shown == this) s_shown = nullptr;
}

void TabBase::onChildFocusGained(aurore::View* directChild, aurore::View* focusedView)
{
    aurore::Box::onChildFocusGained(directChild, focusedView);
    this->focused_once = true;
}

void TabBase::update_ro_note()
{
    if (auto* note = dynamic_cast<aurore::Label*>(this->getView("tab_ro_note"))) ui::set_visible(note, app::read_only());
}

void TabBase::willAppear(bool resetState)
{
    aurore::Box::willAppear(resetState);
    s_shown = this;
    for (aurore::View* v = this; v; v = v->hasParent() ? v->getParent() : nullptr)
        if (v->getId() == "main_tabs") s_main_screen = this->screen;
    frame_stats_flow::set_screen(this->screen);
    this->update_ro_note();
    this->refresh();
    // Opened (not back from a dialog or a screen over it): the tab rises
    // into place, its lines in cascade after it (cadran::List::apply).
    if (!this->revealed) {
        this->revealed = true;
        motion::reveal(this, 6.0f, 160);
    }
}

bool TabBase::shown() const
{
    return s_shown == this;
}

std::string TabBase::shown_screen()
{
    return s_shown ? s_shown->screen : "";
}

std::string TabBase::main_screen()
{
    return s_main_screen;
}

void TabBase::refresh_shown()
{
    if (!s_shown) return;
    s_shown->update_ro_note();
    s_shown->refresh();
}

void TabBase::enable_auto_refresh(int period_ms)
{
    this->auto_timer.setPeriod(period_ms);
    this->auto_timer.setCallback([this]() {
        if (!app::in_focus()) return;
        if (!this->shown()) return;   // replaced, waiting to be freed
        if (!ui::on_top(this->getParentActivity())) return;
        this->refresh();
    });
    this->auto_timer.start();
}
