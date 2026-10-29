// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/notices_activity.hpp"

#include <string>
#include <vector>

#include <aurore/cadran/ui/cadran_list.hpp>

#include "ui/ui.hpp"

using namespace aurore::literals;

namespace
{
// What an entry says below its title.
aurore::Label* detail_label(const std::string& detail)
{
    auto* text = new aurore::Label();
    text->setSingleLine(false);
    text->setFontSize(18);
    text->setThemeColor("textColor", "cadran/text3");
    text->setText(detail);
    text->setMarginLeft(16);
    text->setMarginRight(16);
    text->setMarginTop(8);
    text->setMarginBottom(12);
    return text;
}
}   // namespace

NoticesActivity::~NoticesActivity()
{
    *this->alive = false;
    notices::unlisten(this->listener);
}

void NoticesActivity::open()
{
    aurore::Logger::info("notices: {} to do, {} for information", notices::action_count(), notices::info_count());
    if (notices::list().empty()) {
        ui::notify("playshelf/notices/none"_i18n);
        return;
    }
    aurore::Application::pushActivity(new NoticesActivity());
}

void NoticesActivity::onContentAvailable()
{
    // Every entry for information put away at once.
    this->getContentView()->registerAction("playshelf/notices/read_all"_i18n, aurore::BUTTON_Y, [](aurore::View*) {
        notices::dismiss_all();
        return true;
    });
    // A screen's margins, as the other screens have.
    root->setPadding(10, 72, 32, 72);
    this->rebuild();
    // A change while the list is open: rebuilt on the next frame, outside
    // the source's own call (`alive`: the screen can be gone by then). Under
    // a dialog: once back on top, as the dialog gives the focus back to a
    // cell a rebuild deletes.
    this->listener = notices::listen([this]() {
        aurore::sync(ui::guarded(this->alive, [this]() {
            if (this->closing) return;
            if (ui::has_focus(this)) this->rebuild();
            else this->stale = true;
        }));
    });
}

void NoticesActivity::onResume()
{
    if (!this->stale) return;
    this->stale = false;
    aurore::sync(ui::guarded(this->alive, [this]() { if (!this->closing) this->rebuild(); }));
}

// An entry: its title (amber when it is to do), and A for its action, or to
// put it away. X puts one for information away.
aurore::DetailCell* NoticesActivity::entry_cell(const notices::Entry& e, bool todo)
{
    auto* cell = new aurore::DetailCell();
    cell->setText(e.item.title);
    if (todo) cell->title->setThemeColor("textColor", "cadran/amber");
    cell->detail->setThemeColor("textColor", "cadran/text2");
    const bool has_action = !e.action_label.empty() && e.action;
    cell->setDetailText(has_action ? e.action_label + "  ›" : todo ? "" : "playshelf/notices/seen"_i18n);
    const std::string id = e.item.id;
    const auto action = e.action;
    cell->registerClickAction([this, id, todo, has_action, action](aurore::View*) {
        // Information acted on is read.
        if (!todo) notices::dismiss(id);
        if (!has_action) return true;
        this->closing = true;
        aurore::Application::popActivity(aurore::TransitionAnimation::FADE, [action]() { action(); });
        return true;
    });
    if (!todo)
        cell->registerAction("playshelf/notices/seen"_i18n, aurore::BUTTON_X, [id](aurore::View*) {
            notices::dismiss(id);
            return true;
        });
    return cell;
}

void NoticesActivity::rebuild()
{
    const auto all = notices::list();
    if (all.empty()) {
        // Nothing left: back to the main screen, once.
        this->closing = true;
        aurore::Application::popActivity();
        ui::notify("playshelf/notices/none"_i18n);
        return;
    }

    // New cells in a new box, the focus moved to it, then the old box
    // removed: Aurore would keep a pointer to a deleted focused cell.
    auto* box = new aurore::Box(aurore::Axis::COLUMN);
    box->setAlignItems(aurore::AlignItems::STRETCH);
    aurore::View* first = nullptr;
    std::vector<std::string> ids;
    std::vector<bool>        todo_of;
    int section = -1;
    for (const notices::Entry& e : all) {
        const bool todo = e.item.level == notices::Level::Action;
        if ((int)todo != section) {
            section = todo;
            auto* h = new aurore::Header();
            h->setTitle(todo ? "playshelf/notices/section_todo"_i18n : "playshelf/notices/section_info"_i18n);
            h->setMarginTop(first ? 24 : 0);
            h->setMarginBottom(8);
            box->addView(h);
        }
        aurore::DetailCell* cell = this->entry_cell(e, todo);
        ids.push_back("notice_line_" + std::to_string(ids.size()));
        todo_of.push_back(todo);
        cell->setId(ids.back());
        box->addView(cell);
        if (!first) first = cell;
        if (!e.item.detail.empty()) box->addView(detail_label(e.item.detail));
    }

    // The entries in cards, before they are shown: a warning for what is to
    // do (amber), an "i" for what is only said, so the colour is not alone.
    std::vector<std::pair<const char*, const char*>> icons;
    for (size_t i = 0; i < ids.size(); i++) icons.push_back({ ids[i].c_str(), todo_of[i] ? "alert" : "info" });
    cadran::List(box, box).apply(icons);
    for (size_t i = 0; i < ids.size(); i++)
        if (todo_of[i])
            cadran::List::recolor_icon(static_cast<aurore::DetailCell*>(box->getView(ids[i])), nullptr, "cadran/amber",
                                       "cadran/amber_surf");
    root->addView(box);
    aurore::Application::giveFocus(first);
    if (this->content) root->removeView(this->content);
    this->content = box;
}
