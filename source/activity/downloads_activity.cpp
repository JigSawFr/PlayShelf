// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/downloads_activity.hpp"

#include <fmt/format.h>

#include "action/download_flow.hpp"
#include "action/library_flow.hpp"
#include "action/library_icons.hpp"
#include "ui/cadran_list.hpp"
#include "ui/cadran_modal.hpp"
#include "ui/format_text.hpp"
#include "ui/guarded.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

namespace
{
constexpr float ICON = 72;

const download::Job* job_of(const std::string& id)
{
    for (const download::Job& j : download_flow::jobs())
        if (j.id == id) return &j;
    return nullptr;
}

// "34 % · 1,2 GB of 3,4 GB · 12 MB/s · 3 min left"
std::string running_text(const download_flow::Live& l)
{
    const int   percent = l.total ? (int)std::min<uint64_t>(100, l.done * 100 / l.total) : 0;
    std::string text = brls::getStr("playshelf/downloads/progress", percent, library_flow::size_text(l.done),
                                    library_flow::size_text(l.total));
    if (l.speed > 0) text += " · " + brls::getStr("playshelf/downloads/speed", library_flow::size_text((uint64_t)l.speed));
    if (l.eta) text += " · " + brls::getStr("playshelf/downloads/eta", ui::fmt_duration((uint64_t)*l.eta));
    return text;
}
}   // namespace

DownloadsActivity::~DownloadsActivity()
{
    *this->alive = false;
    download_flow::unlisten(this->listener);
}

void DownloadsActivity::onContentAvailable()
{
    if (auto* applet = dynamic_cast<brls::AppletFrame*>(this->getContentView()))
        applet->setTitle("playshelf/downloads/title"_i18n);

    cadran::kit::Empty empty = cadran::kit::empty("download");
    empty.title->setText("playshelf/downloads/empty_title"_i18n);
    empty.text->setText("playshelf/downloads/empty_text"_i18n);
    empty_box->addView(empty.box);

    keep_awake->init("playshelf/downloads/keep_awake"_i18n, config::get().keep_awake, [](bool on) {
        config::get().keep_awake = on;
        ui::save_config();
        download_flow::keep_awake_changed();
    });
    cadran::List::row(keep_awake, "bolt", true);
    // The margins of the screens the skin dresses (this one dresses itself).
    const cadran::List margins(this->getContentView(), true);
    (void)margins;

    this->listener = download_flow::listen(ui::guarded(this->alive, [this]() { this->update(); }));
    this->set_hints();
    this->update();
}

void DownloadsActivity::set_hints()
{
    const auto& jobs = download_flow::jobs();
    const bool  any_paused = std::any_of(jobs.begin(), jobs.end(), [](const download::Job& j) { return j.state == download::State::Paused; });
    const bool  any_done = std::any_of(jobs.begin(), jobs.end(), [](const download::Job& j) { return j.state == download::State::Done; });
    const bool  any_active = download::pending(jobs) > (size_t)std::count_if(jobs.begin(), jobs.end(), [](const download::Job& j) {
                                return j.state == download::State::Paused;
                            });
    brls::View* root = this->getContentView();
    const std::string x_hint = any_active ? "playshelf/downloads/pause_all"_i18n : any_paused ? "playshelf/downloads/resume_all"_i18n : "";
    root->registerAction(x_hint, brls::BUTTON_X, [](brls::View*) {
                             const auto& all = download_flow::jobs();
                             const bool  active = std::any_of(all.begin(), all.end(), [](const download::Job& j) {
                                 return j.state == download::State::Waiting || j.state == download::State::Running;
                             });
                             if (active) download_flow::pause_all();
                             else download_flow::resume_all();
                             return true;
                         }, x_hint.empty());
    root->registerAction("playshelf/downloads/clear"_i18n, brls::BUTTON_Y, [](brls::View*) {
        download_flow::clear_finished();
        return true;
    }, !any_done);
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void DownloadsActivity::update()
{
    const auto& jobs = download_flow::jobs();
    const auto  groups = download::groups(jobs);
    bool        same = groups.size() == this->rows.size();
    for (size_t i = 0; same && i < groups.size(); i++) {
        same = groups[i].jobs.size() == this->rows[i].ids.size();
        for (size_t k = 0; same && k < groups[i].jobs.size(); k++) same = jobs[groups[i].jobs[k]].id == this->rows[i].ids[k];
    }
    if (!same) {
        // The focus moves off the cards first: they are made again.
        brls::View* focus = brls::Application::getCurrentFocus();
        for (brls::View* v = focus; v; v = v->getParent())
            if (v == list) {
                brls::Application::giveFocus(this->getContentView());
                break;
            }
        list->clearViews(true);
        this->rows.clear();
        this->generation++;
        for (const download::Group& g : groups) {
            Row r;
            for (size_t i : g.jobs) r.ids.push_back(jobs[i].id);
            r.card = cadran::kit::card(true, false);
            r.card->setAxis(brls::Axis::ROW);
            r.card->setAlignItems(brls::AlignItems::CENTER);
            r.card->setPadding(16, 24, 16, 16);
            r.card->setMarginBottom(12);
            // The game's icon, on the well until (and unless) the cache has it.
            auto* cover = new brls::Box(brls::Axis::ROW);
            cover->setWidth(ICON);
            cover->setHeight(ICON);
            cover->setShrink(0);
            cover->setCornerRadius(cadran::kit::RADIUS_ICON);
            cover->setBackgroundColor(cadran::kit::color("cadran/well"));
            cover->setAlignItems(brls::AlignItems::CENTER);
            cover->setJustifyContent(brls::JustifyContent::CENTER);
            cover->setMarginRight(20);
            cover->addView(cadran::kit::icon("gamepad", 36, "cadran/text3"));
            r.icon = new brls::Image();
            r.icon->setWidth(ICON);
            r.icon->setHeight(ICON);
            r.icon->setCornerRadius(cadran::kit::RADIUS_ICON);
            r.icon->setScalingType(brls::ImageScalingType::FIT);
            r.icon->setVisibility(brls::Visibility::GONE);
            cover->addView(r.icon);
            r.card->addView(cover);
            auto* text = new brls::Box(brls::Axis::COLUMN);
            text->setGrow(1.0f);
            text->setWidth(0);
            r.name = cadran::kit::label(g.game, cadran::kit::TEXT_BODY);
            r.name->setSingleLine(true);
            text->addView(r.name);
            r.state = cadran::kit::label(" ", cadran::kit::TEXT_NOTE, "cadran/text3");
            r.state->setSingleLine(true);
            r.state->setMarginTop(6);
            text->addView(r.state);
            r.timeline = new Timeline();
            r.timeline->setMarginTop(10);
            text->addView(r.timeline);
            r.card->addView(text);
            const std::vector<std::string> ids = r.ids;
            r.card->registerClickAction([this, ids](brls::View*) {
                this->open_game(ids);
                return true;
            });
            list->addView(r.card);
            const size_t at = this->rows.size();
            const int    gen = this->generation;
            library_icons::cached(g.icon_url, ui::guarded(this->alive, [this, at, gen](int texture) {
                if (gen != this->generation || at >= this->rows.size() || !texture) {
                    if (texture) nvgDeleteImage(brls::Application::getNVGContext(), texture);
                    return;
                }
                Row& row = this->rows[at];
                row.icon->innerSetImage(texture);
                row.icon->setFreeTexture(true);
                row.icon->setVisibility(brls::Visibility::VISIBLE);
                row.icon->getParent()->getChildren()[0]->setVisibility(brls::Visibility::GONE);
            }));
            this->rows.push_back(r);
        }
        if (!this->rows.empty() && !brls::Application::getCurrentFocus()->isFocusable())
            brls::Application::giveFocus(this->rows.front().card);
    }
    ui::set_visible(empty_box, jobs.empty());
    for (Row& r : this->rows) this->fill(r);
    this->set_hints();
}

void DownloadsActivity::fill(Row& row)
{
    const auto& jobs = download_flow::jobs();
    download::Group g;
    uint64_t        size = 0;
    for (const std::string& id : row.ids)
        for (size_t i = 0; i < jobs.size(); i++)
            if (jobs[i].id == id) {
                g.jobs.push_back(i);
                size += jobs[i].size;
            }
    if (g.jobs.empty()) return;
    const download_flow::Live live = download_flow::live();
    row.timeline->set(download::timeline(g, jobs, live.id, live.done, live.total));

    // Where the game stands: the package being installed, else the line's state.
    std::string  text;
    const char*  color = "cadran/text3";
    const size_t count = g.jobs.size();
    const std::string packages = count == 1 ? "playshelf/downloads/packages_one"_i18n
                                            : brls::getStr("playshelf/downloads/packages", count);
    switch (download::group_state(g, jobs)) {
    case download::State::Running:
        for (size_t i : g.jobs)
            if (jobs[i].id == live.id) {
                const std::string what = jobs[i].label.empty() ? jobs[i].name : jobs[i].label;
                text = what + " · " + (live.total ? running_text(live) : "playshelf/downloads/state_starting"_i18n);
            }
        color = "cadran/teal";
        break;
    case download::State::Waiting:
        text = "playshelf/downloads/state_waiting"_i18n + " · " + packages;
        break;
    case download::State::Paused:
        text = "playshelf/downloads/state_paused"_i18n + " · " + packages;
        color = "cadran/amber";
        break;
    case download::State::Done:
        text = "playshelf/downloads/state_done"_i18n + " · " + packages;
        color = "cadran/teal";
        break;
    case download::State::Failed:
        for (size_t i : g.jobs)
            if (jobs[i].state == download::State::Failed && text.empty()) {
                const std::string what = jobs[i].label.empty() ? jobs[i].name : jobs[i].label;
                text = what + " · " + (jobs[i].unsigned_content ? "playshelf/downloads/state_unsigned"_i18n
                                                               : brls::getStr("playshelf/downloads/state_failed", jobs[i].error));
            }
        color = "cadran/coral";
        break;
    }
    if (size && download::group_state(g, jobs) != download::State::Running) text += " · " + library_flow::size_text(size);
    row.state->setTextColor(cadran::kit::color(color));
    ui::set_text(row.state, text);
}

void DownloadsActivity::open_game(const std::vector<std::string>& ids)
{
    std::vector<const download::Job*> pkgs;
    for (const std::string& id : ids)
        if (const download::Job* j = job_of(id)) pkgs.push_back(j);
    if (pkgs.empty()) return;
    if (pkgs.size() == 1) {
        this->open_job(pkgs[0]->id);
        return;
    }
    using Choice = ui::Choice;
    std::vector<Choice> choices;
    const download_flow::Live live = download_flow::live();
    bool                      active = false, stopped = false;
    for (const download::Job* j : pkgs) {
        active = active || j->state == download::State::Waiting || j->state == download::State::Running;
        stopped = stopped || j->state == download::State::Paused || j->state == download::State::Failed;
    }
    if (active)
        choices.emplace_back("playshelf/downloads/pause_game"_i18n, [ids]() {
            for (const std::string& id : ids) download_flow::pause(id);
        }, "pause");
    if (stopped)
        choices.emplace_back("playshelf/downloads/resume_game"_i18n, [ids]() {
            for (const std::string& id : ids) download_flow::resume(id);
        }, "download");
    // Each package, its state at the end; A: its own actions.
    for (const download::Job* j : pkgs) {
        std::string state;
        Choice::Tone tone = Choice::Tone::PLAIN;
        switch (j->state) {
        case download::State::Waiting: state = "playshelf/downloads/state_waiting"_i18n; break;
        case download::State::Running:
            state = live.total ? fmt::format("{} %", (int)std::min<uint64_t>(100, live.done * 100 / live.total))
                               : "playshelf/downloads/state_starting"_i18n;
            tone = Choice::Tone::ON;
            break;
        case download::State::Paused: state = "playshelf/downloads/state_paused"_i18n; tone = Choice::Tone::WARN; break;
        case download::State::Done: state = "playshelf/downloads/state_done"_i18n; tone = Choice::Tone::ON; break;
        case download::State::Failed: state = "playshelf/downloads/failed_short"_i18n; tone = Choice::Tone::DANGER; break;
        }
        const std::string id = j->id;
        choices.emplace_back(j->label.empty() ? j->name : j->label,
                             ui::guarded(this->alive, [this, id]() { this->open_job(id); }), "file", state,
                             "playshelf/downloads/group_packages"_i18n, tone);
    }
    const std::string game = pkgs[0]->game.empty() ? pkgs[0]->name : pkgs[0]->game;
    choices.emplace_back("playshelf/downloads/remove_game"_i18n, [ids, game]() {
        ui::confirm_danger(brls::getStr("playshelf/downloads/remove_game_body", game), "playshelf/downloads/remove"_i18n,
                           [ids]() {
                               for (const std::string& id : ids) download_flow::cancel(id);
                           });
    }, "trash", "", "", Choice::Tone::DANGER);
    uint64_t size = 0;
    for (const download::Job* j : pkgs) size += j->size;
    ui::pick_actions(game, choices, size ? library_flow::size_text(size) : "");
}

void DownloadsActivity::open_job(const std::string& id)
{
    const download::Job* j = job_of(id);
    if (!j) return;
    using Choice = ui::Choice;
    std::vector<Choice> choices;
    switch (j->state) {
    case download::State::Waiting:
    case download::State::Running:
        choices.emplace_back("playshelf/downloads/pause"_i18n, [id]() { download_flow::pause(id); }, "pause");
        break;
    case download::State::Paused:
        choices.emplace_back("playshelf/downloads/resume"_i18n, [id]() { download_flow::resume(id); }, "download");
        break;
    case download::State::Failed:
        if (j->unsigned_content)
            choices.emplace_back("playshelf/downloads/install_anyway"_i18n, [id]() { download_flow::accept_unsigned(id); },
                                 "alert", "", "", Choice::Tone::WARN);
        else
            choices.emplace_back("playshelf/downloads/retry"_i18n, [id]() { download_flow::resume(id); }, "refresh");
        break;
    case download::State::Done:
        break;
    }
    const bool        done = j->state == download::State::Done;
    const std::string name = j->name;
    choices.emplace_back(done ? "playshelf/downloads/forget"_i18n : "playshelf/downloads/remove"_i18n,
                         [id, name, done]() {
                             if (done) {
                                 download_flow::cancel(id);
                                 return;
                             }
                             ui::confirm_danger(brls::getStr("playshelf/downloads/remove_body", name),
                                                "playshelf/downloads/remove"_i18n, [id]() { download_flow::cancel(id); });
                         },
                         "trash", "", "", done ? Choice::Tone::PLAIN : Choice::Tone::DANGER);
    std::string subtitle = j->state == download::State::Failed && j->unsigned_content ? "playshelf/downloads/unsigned_note"_i18n : "";
    ui::pick_actions(name, choices, subtitle);
}
