// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/downloads_activity.hpp"

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/ui/cadran_modal.hpp>
#include <borealis/cadran/ui/format_text.hpp>
#include <borealis/cadran/ui/guarded.hpp>

#include "action/download_flow.hpp"
#include "action/library_flow.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

using namespace brls::literals;

namespace
{
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
    bool        same = jobs.size() == this->rows.size();
    for (size_t i = 0; same && i < jobs.size(); i++) same = jobs[i].id == this->rows[i].id;
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
        for (const download::Job& j : jobs) {
            Row r;
            r.id = j.id;
            r.card = cadran::kit::card(true, false);
            r.card->setAxis(brls::Axis::COLUMN);
            r.card->setPadding(18, 24, 18, 24);
            r.card->setMarginBottom(12);
            r.name = cadran::kit::label(j.name, cadran::kit::TEXT_BODY);
            r.name->setSingleLine(true);
            r.card->addView(r.name);
            r.state = cadran::kit::label(" ", cadran::kit::TEXT_NOTE, "cadran/text3");
            r.state->setMarginTop(6);
            r.card->addView(r.state);
            r.bar = new cadran::kit::Bar();
            r.bar->setMarginTop(10);
            r.card->addView(r.bar);
            const std::string id = j.id;
            r.card->registerClickAction([this, id](brls::View*) {
                this->open_job(id);
                return true;
            });
            list->addView(r.card);
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
    const download::Job* j = job_of(row.id);
    if (!j) return;
    std::string text;
    const char* color = "cadran/text3";
    float       part = j->total ? (float)((double)j->done / (double)j->total) : 0.0f;
    switch (j->state) {
    case download::State::Waiting:
        text = "playshelf/downloads/state_waiting"_i18n;
        break;
    case download::State::Running: {
        const download_flow::Live l = download_flow::live();
        text = l.total ? running_text(l) : "playshelf/downloads/state_starting"_i18n;
        part = l.total ? (float)((double)l.done / (double)l.total) : 0.0f;
        color = "cadran/teal";
        break;
    }
    case download::State::Paused:
        text = "playshelf/downloads/state_paused"_i18n;
        color = "cadran/amber";
        break;
    case download::State::Done:
        text = "playshelf/downloads/state_done"_i18n;
        color = "cadran/teal";
        part = 1.0f;
        break;
    case download::State::Failed:
        text = j->unsigned_content ? "playshelf/downloads/state_unsigned"_i18n
                                   : brls::getStr("playshelf/downloads/state_failed", j->error);
        color = "cadran/coral";
        break;
    }
    if (j->size && j->state != download::State::Running) text += " · " + library_flow::size_text(j->size);
    row.state->setTextColor(cadran::kit::color(color));
    ui::set_text(row.state, text);
    row.bar->set(part);
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
