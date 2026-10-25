// ui — read-only greying, showing / hiding views with the focus, tabs, the unlock banner.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include "app.hpp"
#include "ui/cadran_list.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/hooks.hpp"

#include <algorithm>
#include <yoga/Yoga.h>

using namespace brls::literals;

namespace ui
{

bool refuse_read_only()
{
    if (!app::read_only()) return false;
    refuse(NXM_RC_READ_ONLY);
    return true;
}

void show_writable(brls::DetailCell* cell, bool writable, NVGcolor title, NVGcolor detail)
{
    const NVGcolor grey = nvgTransRGBA(cadran::kit::color("cadran/text"), 110);
    cell->title->setTextColor(writable ? title : grey);
    if (auto* sw = dynamic_cast<brls::BooleanCell*>(cell)) {
        if (writable) sw->setOn(sw->isOn(), false);   // its own value colours back
        else sw->detail->setTextColor(grey);
        return;
    }
    cell->detail->setTextColor(writable ? detail : grey);
}

void show_writable(brls::DetailCell* cell, bool writable)
{
    show_writable(cell, writable, cadran::kit::color("cadran/text"), cadran::kit::color("cadran/text2"));
}

void guard_switch(brls::BooleanCell* cell)
{
    cell->registerClickAction([cell](brls::View*) {
        if (refuse_read_only()) return true;
        cell->setOn(!cell->isOn());
        cell->getEvent()->fire(cell->isOn());
        return true;
    });
}

void set_visible(brls::View* view, bool visible)
{
    if (!view) return;
    // An empty state's label (kit::empty_around): its whole card.
    if (brls::View* card = view->getParent(); card && card->getId() == "cadran_empty_card") view = card;
    const bool had_focus = !visible && view->isFocused();
    view->setVisibility(visible ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (auto* card = dynamic_cast<cadran::Card*>(view->getParent())) card->follow_rows();
    if (!had_focus) return;

    // Never leave the focus on a view that just disappeared: prefer the
    // nearest focusable sibling, then the nearest one of each enclosing box in
    // turn (the line may sit in a card of its own), then the container.
    brls::Box* parent = view->getParent();
    if (!parent) return;
    for (brls::View* node = view; node->getParent(); node = node->getParent()) {
        const auto& children = node->getParent()->getChildren();
        const int count = (int)children.size();
        int index = -1;
        for (int i = 0; i < count; i++)
            if (children[i] == node) index = i;
        for (int distance = 1; index >= 0 && distance < count; distance++) {
            for (int i : { index + distance, index - distance }) {
                if (i < 0 || i >= count) continue;
                if (brls::View* target = children[i]->getDefaultFocus()) {
                    brls::Application::giveFocus(target);
                    return;
                }
            }
        }
        // Not past the scrolling frame: the rest of the screen is another list.
        if (dynamic_cast<brls::ScrollingFrame*>(node->getParent())) break;
    }
    brls::Application::giveFocus(parent);
}

void set_visible_all(std::initializer_list<std::pair<brls::View*, bool>> changes)
{
    for (const auto& c : changes)
        if (c.second) set_visible(c.first, true);
    for (const auto& c : changes)
        if (!c.second) set_visible(c.first, false);
}

void set_text(brls::Label* label, const std::string& text)
{
    if (label && label->getFullText() != text) label->setText(text);
}

void set_detail(brls::DetailCell* cell, const std::string& text)
{
    if (cell) set_text(cell->detail, text);
}

void batch_layout(brls::Box* box, const std::function<void()>& fill)
{
    // Out of its parent's Yoga node only: borealis' views, their parents and
    // the focus stay as they are, and a change inside stops at the box.
    YGNodeRef node = box ? box->getYGNode() : nullptr;
    YGNodeRef owner = node ? YGNodeGetOwner(node) : nullptr;
    uint32_t index = 0;
    while (owner && index < YGNodeGetChildCount(owner) && YGNodeGetChild(owner, index) != node) index++;
    if (!owner || index == YGNodeGetChildCount(owner)) {
        fill();
        return;
    }
    YGNodeRemoveChild(owner, node);
    fill();
    YGNodeInsertChild(owner, node, index);
    box->invalidate();
}

void keep_focus(brls::Activity* activity, brls::View* fallback)
{
    // The view to get the focus back: the end of the boxes' last focused
    // views. Box::removeView leaves a removed child there: the walk stops
    // at one no longer among its box's children (never read).
    brls::View* last = activity->getContentView();
    while (auto* box = dynamic_cast<brls::Box*>(last)) {
        brls::View* next = box->getLastFocusedView();
        const auto& children = box->getChildren();
        if (!next || std::find(children.begin(), children.end(), next) == children.end()) break;
        last = next;
    }
    bool gone = !last || !last->isFocusable();
    for (brls::View* v = last; v && !gone; v = v->getParent()) gone = v->getVisibility() != brls::Visibility::VISIBLE;
    if (gone) brls::Application::giveFocus(fallback ? fallback : activity->getContentView());
}

bool has_focus(brls::Activity* activity)
{
    brls::View* focus = brls::Application::getCurrentFocus();
    return focus && focus->getParentActivity() == activity;
}

bool on_top(brls::Activity* activity)
{
    const auto& stack = brls::Application::getActivitiesStack();
    return !stack.empty() && stack.back() == activity;
}

brls::Activity* main_screen()
{
    const auto& is_main = hooks::get().is_main;
    if (!is_main) return nullptr;
    for (brls::Activity* a : brls::Application::getActivitiesStack())
        if (is_main(a)) return a;
    return nullptr;
}

void open_tab(int position, const std::string& row)
{
    brls::Activity* main = main_screen();
    if (main && hooks::get().open_tab) hooks::get().open_tab(main, position, row);
}

void refresh_shown_tab()
{
    if (hooks::get().refresh_shown) hooks::get().refresh_shown();
}

void go_to_tab(brls::View* from, int position)
{
    brls::TabFrame* frame = nullptr;
    for (brls::View* v = from; v && !frame; v = v->getParent())
        frame = dynamic_cast<brls::TabFrame*>(v);
    if (!frame) return;
    brls::sync([frame, position]() {
        frame->focusTab(position);   // replaces the tab content
        const auto& children = frame->getChildren();
        if (children.size() >= 2) brls::Application::giveFocus(children.back());
    });
}

void open_screen(brls::View* content, const std::string& title)
{
    auto* frame = new brls::AppletFrame(content);
    frame->setTitle(title);
    frame->setIcon(brls::View::getFilePathXMLAttributeValue("@res/img/app_icon.jpg"));
    brls::Application::pushActivity(new brls::Activity(frame));
}

}   // namespace ui
