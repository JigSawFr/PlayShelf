// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/dev_builds_activity.hpp"

#include <utility>

#include <fmt/format.h>

#include <aurore/cadran/view/cadran_icon.hpp>

#include "action/dev_build_flow.hpp"
#include "action/rollback_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "view/build_cell.hpp"

using namespace aurore::literals;

namespace
{
DevBuildsActivity* s_open = nullptr;   // the screen, while it is open
// The tab last chosen in this run; none yet: the installed build's.
bool              s_tab_chosen = false;
dev_builds::Kind  s_tab = dev_builds::Kind::Release;

constexpr dev_builds::Kind KINDS[] = { dev_builds::Kind::Release, dev_builds::Kind::Main,
                                       dev_builds::Kind::PullRequest };

dev_builds::Order order()
{
    return dev_builds::order_named(config::get().dev_builds_order);
}

// After a tab's words: a sparkle and how many came after the running build
// (the sparkle of their tag in the list).
struct NewCount
{
    aurore::Box*   box;
    CadranIcon*  glyph;
    aurore::Label* count;
};

NewCount add_new_count(aurore::Button* tab)
{
    auto* box = new aurore::Box(aurore::Axis::ROW);
    box->setAlignItems(aurore::AlignItems::CENTER);
    box->setMarginLeft(10);
    auto* glyph = new CadranIcon();
    glyph->setIcon("sparkle");
    glyph->setWidth(18);
    glyph->setHeight(18);
    glyph->setMarginRight(4);
    box->addView(glyph);
    auto* count = new aurore::Label();
    count->setSingleLine(true);
    count->setFontSize(20);
    box->addView(count);
    tab->addView(box);
    return { box, glyph, count };
}

// `n` new in that tab's ink (dark on the chosen tab's teal); none: gone.
void set_new_count(const NewCount& c, size_t n, bool chosen)
{
    const std::string& ink = (chosen ? aurore::BUTTONSTYLE_PRIMARY : aurore::BUTTONSTYLE_DEFAULT).enabledLabelColor;
    c.glyph->setThemeColor("color", ink);
    c.count->setThemeColor("textColor", ink);
    c.count->setText(std::to_string(n));
    ui::set_visible(c.box, n > 0);
}

NewCount s_new_release, s_new_main;   // the open screen's (s_open)
}   // namespace

void DevBuildsActivity::show(const dev_builds::Cache& cache)
{
    if (s_open) {
        s_open->update(cache);
        return;
    }
    aurore::Application::pushActivity(new DevBuildsActivity(cache));
}

void DevBuildsActivity::loading(const std::string& text, float done, bool steady)
{
    if (!s_open) return;
    DevBuildsActivity* a = s_open;
    if (a->loading_box->getVisibility() != aurore::Visibility::VISIBLE) {
        a->loading_bar->reset();
        ui::set_visible(a->loading_box.getView(), true);
    }
    a->loading_text->setText(text);
    ui::set_visible(a->loading_bar.getView(), true);
    // Steady: most of the way over ten seconds, the end left for the answer.
    if (steady) a->loading_bar->fillOver(0.9f, 10000000);
    else a->loading_bar->setProgress(done);
}

void DevBuildsActivity::loaded()
{
    if (s_open) ui::set_visible(s_open->loading_box.getView(), false);
}

void DevBuildsActivity::failed(const std::string& text)
{
    if (!s_open) return;
    ui::set_visible(s_open->loading_box.getView(), true);
    ui::set_visible(s_open->loading_bar.getView(), false);
    s_open->loading_text->setText(text);
}

DevBuildsActivity::DevBuildsActivity(const dev_builds::Cache& fetched) : cache(fetched)
{
    s_open = this;
    if (s_tab_chosen) {
        this->tab = s_tab;
        return;
    }
    for (const dev_builds::Build& b : fetched.builds)
        if (dev_build_flow::installed(b)) this->tab = b.kind;
}

DevBuildsActivity::~DevBuildsActivity()
{
    if (s_open == this) s_open = nullptr;
}

void DevBuildsActivity::onContentAvailable()
{
    note->setSingleLine(false);
    loading_text->setSingleLine(false);
    // The bar in the header, at its right (the room the title leaves there),
    // not over the tabs.
    auto* frame = dynamic_cast<aurore::AppletFrame*>(this->getContentView());
    if (aurore::Box* header = frame ? frame->getHeader() : nullptr) {
        if (aurore::Box* parent = loading_box->getParent()) parent->removeView(loading_box.getView(), false);
        loading_box->setAlignSelf(aurore::AlignSelf::CENTER);
        loading_box->setMarginRight(16);
        header->addView(loading_box.getView(), 1);
    }
    // Not the running build: the header's badge names it already, the list
    // marks it, and the build's page and the download name it again.
    note->setText("playshelf/dev_build/note"_i18n);
    const std::pair<aurore::Button*, dev_builds::Kind> buttons[] = { { tab_release.getView(), KINDS[0] },
                                                                   { tab_main.getView(), KINDS[1] },
                                                                   { tab_pulls.getView(), KINDS[2] } };
    for (const auto& b : buttons) {
        const dev_builds::Kind kind = b.second;
        b.first->registerClickAction([this, kind](aurore::View*) {
            // A on the pull requests' tab when it is the one shown: the other
            // order (the tab says which); else, that tab.
            if (kind == dev_builds::Kind::PullRequest && this->tab == kind) this->sort();
            else this->show_tab(kind, false);
            return true;
        });
    }
    sign_in->registerClickAction([](aurore::View*) {
        dev_build_flow::sign_in();
        return true;
    });
    back->registerClickAction([](aurore::View*) {
        dev_build_flow::go_back();
        return true;
    });
    // The L and R icons around the tabs say what moves between them (the same
    // glyphs as the footer's hints); no hint of their own in the footer.
    s_new_release = add_new_count(tab_release.getView());
    s_new_main = add_new_count(tab_main.getView());
    tab_l->setText(aurore::Hint::getKeyIcon(aurore::BUTTON_LB, true));
    tab_r->setText(aurore::Hint::getKeyIcon(aurore::BUTTON_RB, true));
    this->getContentView()->registerAction("", aurore::BUTTON_RB, [this](aurore::View*) {
        this->next_tab(1);
        return true;
    }, true);
    this->getContentView()->registerAction("", aurore::BUTTON_LB, [this](aurore::View*) {
        this->next_tab(-1);
        return true;
    }, true);
    // Ⓨ's place among the hints (before Ⓧ); fill() sets what it does.
    this->getContentView()->registerAction("", aurore::BUTTON_Y, [](aurore::View*) { return true; }, true);
    this->getContentView()->registerAction("playshelf/hints/refresh"_i18n, aurore::BUTTON_X, [](aurore::View*) {
        dev_build_flow::open(true);
        return true;
    });
    // A build whose checks are still running: the list is fetched again now
    // and then while it is on screen (nothing over it), so its tag follows.
    this->refresh_timer.setPeriod(REFRESH_MS);
    this->refresh_timer.setCallback([this]() {
        if (!ui::has_focus(this)) return;
        for (const dev_builds::Build& b : this->cache.builds)
            if (b.ci == dev_builds::Ci::Running) {
                dev_build_flow::refresh();
                return;
            }
    });
    this->refresh_timer.start();
    this->fill();
    // The screen opens on the first build, not on the tabs above it.
    if (!cells.empty() && cells[0]->getVisibility() == aurore::Visibility::VISIBLE) aurore::Application::giveFocus(cells[0]);
}

void DevBuildsActivity::update(const dev_builds::Cache& fresh)
{
    // Signed in from this screen: its line goes, the focus to the first build.
    // So does a first list, the screen having opened empty under its bar.
    aurore::View* focus = aurore::Application::getCurrentFocus();
    const bool was_empty = this->cache.builds.empty();
    const bool focus_moves = (!fresh.needs_login && focus == sign_in.getView()) || was_empty;
    this->cache = fresh;
    if (was_empty && !s_tab_chosen)
        for (const dev_builds::Build& b : fresh.builds)
            if (dev_build_flow::installed(b)) this->tab = b.kind;
    this->fill();
    // Never under a dialog (Back to the previous build, the sign-in code).
    if (!ui::has_focus(this)) return;
    if (focus_moves && !cells.empty() && cells[0]->getVisibility() == aurore::Visibility::VISIBLE)
        aurore::Application::giveFocus(cells[0]);
}

// Back from a build's screen or a dialog: the list fetched meanwhile may
// have hidden the build it was opened from (fewer builds) or a tab.
void DevBuildsActivity::onResume()
{
    aurore::View* first = cells.empty() ? nullptr : cells[0];
    ui::keep_focus(this, first && first->getVisibility() == aurore::Visibility::VISIBLE ? first : nullptr);
}

std::vector<dev_builds::Build> DevBuildsActivity::builds(dev_builds::Kind kind) const
{
    return dev_builds::of_kind(cache.builds, kind, order());
}

void DevBuildsActivity::fill()
{
    // A tab without builds is left out (signed out: the releases only, and
    // then no tabs at all); the one shown always has some, when any has.
    if (this->builds(this->tab).empty())
        for (dev_builds::Kind k : KINDS)
            if (!this->builds(k).empty()) {
                this->tab = k;
                break;
            }
    const std::vector<dev_builds::Build> releases = this->builds(dev_builds::Kind::Release);
    const std::vector<dev_builds::Build> mains = this->builds(dev_builds::Kind::Main);
    const size_t pulls = this->builds(dev_builds::Kind::PullRequest).size();
    // Newer than the running build: a release with a higher version (on a
    // release build or not), a commit of main built after the running one.
    const dev_builds::Build* running = dev_build_flow::running_build(this->cache.builds);
    auto newer_count = [running](const std::vector<dev_builds::Build>& kind_builds) {
        size_t n = 0;
        for (const dev_builds::Build& b : kind_builds) n += dev_builds::newer(b, app::version(), running) ? 1 : 0;
        return n;
    };
    tab_release->setText(aurore::getStr("playshelf/dev_build/section_release", (int)releases.size()));
    tab_main->setText(aurore::getStr("playshelf/dev_build/section_main", (int)mains.size()));
    set_new_count(s_new_release, newer_count(releases), this->tab == dev_builds::Kind::Release);
    set_new_count(s_new_main, newer_count(mains), this->tab == dev_builds::Kind::Main);
    tab_pulls->setText(aurore::getStr("playshelf/dev_build/section_pulls", (int)pulls));
    // The order in A's hint, on the pull requests' tab when it is shown (what
    // A does there): not in the tab, which it made too wide.
    tab_pulls->updateActionHint(aurore::BUTTON_A, this->tab != dev_builds::Kind::PullRequest
                                                    ? "hints/ok"_i18n
                                                    : aurore::getStr("playshelf/dev_build/sort_hint",
                                                                   order() == dev_builds::Order::Number
                                                                       ? "playshelf/dev_build/order_newest"_i18n
                                                                       : "playshelf/dev_build/order_number"_i18n));
    tab_release->setStyle(this->tab == dev_builds::Kind::Release ? &aurore::BUTTONSTYLE_PRIMARY : &aurore::BUTTONSTYLE_DEFAULT);
    tab_main->setStyle(this->tab == dev_builds::Kind::Main ? &aurore::BUTTONSTYLE_PRIMARY : &aurore::BUTTONSTYLE_DEFAULT);
    tab_pulls->setStyle(this->tab == dev_builds::Kind::PullRequest ? &aurore::BUTTONSTYLE_PRIMARY : &aurore::BUTTONSTYLE_DEFAULT);
    ui::set_visible(tab_release.getView(), !releases.empty());
    ui::set_visible(tab_main.getView(), !mains.empty());
    ui::set_visible(tab_pulls.getView(), pulls > 0);
    aurore::View* focus = aurore::Application::getCurrentFocus();
    const bool tabs_go = mains.empty() && pulls == 0;
    ui::set_visible(tabs.getView(), !tabs_go);
    // Ⓨ looks for the build of any pull request or commit: with a GitHub
    // token only (signed out, no hint: the Sign in line is there instead).
    const bool can_find = !cache.needs_login;
    this->getContentView()->registerAction(can_find ? "playshelf/dev_build/find_hint"_i18n : "", aurore::BUTTON_Y,
                                           [can_find](aurore::View*) {
                                               if (can_find) dev_build_flow::find();
                                               return true;
                                           }, !can_find);
    aurore::Application::getGlobalHintsUpdateEvent()->fire();

    const std::vector<dev_builds::Build> shown = this->builds(this->tab);
    for (size_t i = 0; i < shown.size(); i++) {
        if (i >= cells.size()) {
            cells.push_back(new BuildCell());
            list->addView(cells.back());
        }
        BuildCell* cell = cells[i];
        const dev_builds::Build b = shown[i];
        // Two lines: its title, then its tags (kind of change, checks,
        // installed or new); how old it is and its commit at the right.
        const bool is_newer = dev_builds::newer(b, app::version(), running);
        cell->set(b, is_newer);
        cell->registerClickAction([b, is_newer](aurore::View*) {
            dev_build_flow::choose(b, is_newer);
            return true;
        });
        ui::set_visible(cell, true);
    }
    for (size_t i = shown.size(); i < cells.size(); i++) ui::set_visible(cells[i], false);
    // Never the focus left on a tab that went with its row.
    if (tabs_go && focus && focus->getParent() == tabs.getView() && !shown.empty())
        aurore::Application::giveFocus(cells[0]);
    // Without a token GitHub hands out the releases only: a line signs in.
    ui::set_visible(sign_in.getView(), cache.needs_login);
    // The build installed before this one, kept: a line goes back to it.
    std::string previous;
    const bool have_previous = rollback_flow::previous(&previous);
    if (have_previous) back->setText(aurore::getStr("playshelf/dev_build/back", previous));
    ui::set_visible(back.getView(), have_previous);
    checked->setText(cache.builds.empty() ? " " : dev_build_flow::checked_text(cache));
}

void DevBuildsActivity::show_tab(dev_builds::Kind kind, bool focus_list)
{
    if (this->builds(kind).empty()) return;
    this->tab = kind;
    s_tab = kind;
    s_tab_chosen = true;
    this->fill();
    if (focus_list) aurore::Application::giveFocus(cells[0]);
}

void DevBuildsActivity::next_tab(int step)
{
    int at = std::to_underlying(this->tab);
    for (int n = 0; n < 3; n++) {
        at = (at + step + 3) % 3;
        if (this->builds(KINDS[at]).empty()) continue;
        // From a tab, the focus goes to the tab chosen; else to its first build.
        aurore::View* focus = aurore::Application::getCurrentFocus();
        const bool on_tabs = focus && focus->getParent() == tabs.getView();
        this->show_tab(KINDS[at], !on_tabs);
        if (on_tabs) {
            aurore::Button* b = at == 0 ? tab_release.getView() : at == 1 ? tab_main.getView() : tab_pulls.getView();
            aurore::Application::giveFocus(b);
        }
        return;
    }
}

void DevBuildsActivity::sort()
{
    // The pull requests' tab, the focused one keeping the focus wherever it moves.
    int focused_pr = 0;
    if (this->tab == dev_builds::Kind::PullRequest) {
        const std::vector<dev_builds::Build> before = this->builds(dev_builds::Kind::PullRequest);
        for (size_t i = 0; i < cells.size() && i < before.size(); i++)
            if (cells[i] == aurore::Application::getCurrentFocus()) focused_pr = before[i].pr;
    }

    const bool by_number = order() == dev_builds::Order::Number;
    config::get().dev_builds_order = dev_builds::order_name(by_number ? dev_builds::Order::Newest : dev_builds::Order::Number);
    ui::save_config();
    this->fill();
    ui::notify(by_number ? "playshelf/dev_build/order_newest"_i18n : "playshelf/dev_build/order_number"_i18n);

    if (!focused_pr) return;
    const std::vector<dev_builds::Build> after = this->builds(dev_builds::Kind::PullRequest);
    for (size_t i = 0; i < after.size() && i < cells.size(); i++)
        if (after[i].pr == focused_pr) aurore::Application::giveFocus(cells[i]);
}
