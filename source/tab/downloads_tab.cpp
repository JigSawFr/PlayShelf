// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/downloads_tab.hpp"

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/ui/cadran_modal.hpp>
#include <borealis/cadran/ui/format_text.hpp>
#include <borealis/cadran/ui/guarded.hpp>

#include "action/download_flow.hpp"
#include "action/library_flow.hpp"
#include "action/library_icons.hpp"
#include "install/split_stream.hpp"
#include "titledb/nlib.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "view/banner_card.hpp"
#include "view/cover_placeholder.hpp"

using namespace brls::literals;

namespace
{
constexpr float ICON = 72;

const download::Job* job_of(const std::string& job_id)
{
    for (const download::Job& j : download_flow::jobs())
        if (j.id == job_id) return &j;
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

// "12 s" under a minute, else as ui::fmt_duration words it.
std::string short_duration(double seconds)
{
    const uint64_t s = seconds > 0 ? (uint64_t)seconds : 0;
    return s < 60 ? brls::getStr("playshelf/downloads/seconds", s) : ui::fmt_duration(s);
}

// Why a job failed, in a few words ("network cut"); its own error when
// none fits.
std::string cause_text(const download::Job& j)
{
    const download::Cause c = download::cause_of(j.error, j.unsigned_content);
    if (c == download::Cause::Unsigned) return "playshelf/downloads/state_unsigned"_i18n;
    const std::string why = c == download::Cause::Other ? j.error : brls::getStr(std::string("playshelf/downloads/cause/") + download::cause_name(c));
    const double      in = download_flow::retry_in(j);
    if (in >= 0) return brls::getStr("playshelf/downloads/state_retry", short_duration(in + 1), why);
    return brls::getStr("playshelf/downloads/state_failed", why);
}
}   // namespace

DownloadsTab::DownloadsTab()
    : TabBase("xml/tab/downloads.xml", false)
{
    cadran::kit::Empty empty = cadran::kit::empty("download");
    empty.title->setText("playshelf/downloads/empty_title"_i18n);
    empty.text->setText("playshelf/downloads/empty_text"_i18n);
    empty_box->addView(empty.box);

    // The settings, a card: staying awake, the connections.
    brls::Box* card = cadran::List::rows_in(settings);
    keep_awake = new brls::BooleanCell();
    keep_awake->init("playshelf/downloads/keep_awake"_i18n, config::get().keep_awake, [](bool on) {
        config::get().keep_awake = on;
        ui::save_config();
        download_flow::keep_awake_changed();
    });
    card->addView(keep_awake);
    cadran::List::row(keep_awake, "bolt");
    connections = new brls::DetailCell();
    connections->setText("playshelf/downloads/connections"_i18n);
    ui::set_detail(connections, std::to_string(config::get().connections));
    connections->registerClickAction([this](brls::View*) {
        std::vector<std::string> values, details;
        for (int n = 1; n <= install::MAX_CONNECTIONS; n++) {
            values.push_back(std::to_string(n));
            details.push_back(n == 1   ? "playshelf/downloads/connections_one"_i18n
                              : n == 4 ? "playshelf/downloads/connections_default"_i18n
                              : n == install::MAX_CONNECTIONS ? "playshelf/downloads/connections_most"_i18n
                                                              : "");
        }
        ui::pick("playshelf/downloads/connections"_i18n, values, config::get().connections - 1,
                 ui::guarded(this->alive, [this](int i) {
                     config::get().connections = i + 1;
                     ui::save_config();
                     ui::set_detail(this->connections, std::to_string(i + 1));
                 }),
                 true, details);
        return true;
    });
    card->addView(connections);
    cadran::List::row(connections, "server");
    prefetch = new brls::DetailCell();
    prefetch->setText("playshelf/downloads/prefetch"_i18n);
    auto prefetch_text = [](int n) {
        return n <= 0 ? "playshelf/downloads/prefetch_off"_i18n
             : n == 1 ? "playshelf/downloads/prefetch_one"_i18n
                      : brls::getStr("playshelf/downloads/prefetch_many", n);
    };
    ui::set_detail(prefetch, prefetch_text(config::get().prefetch));
    prefetch->registerClickAction([this, prefetch_text](brls::View*) {
        const std::vector<std::string> values = { prefetch_text(0), prefetch_text(1), prefetch_text(2) };
        ui::pick("playshelf/downloads/prefetch"_i18n, values, config::get().prefetch,
                 ui::guarded(this->alive, [this, prefetch_text](int i) {
                     config::get().prefetch = i;
                     ui::save_config();
                     ui::set_detail(this->prefetch, prefetch_text(i));
                     download_flow::prefetch_changed();
                 }),
                 true, { "", "playshelf/downloads/prefetch_note"_i18n, "playshelf/downloads/prefetch_note"_i18n });
        return true;
    });
    card->addView(prefetch);
    cadran::List::row(prefetch, "folder");
    // A tab's margins, as the other tabs' lists have them.
    const cadran::List margins(this);
    (void)margins;

    this->listener = download_flow::listen(ui::guarded(this->alive, [this]() {
        if (this->shown()) this->update();
    }));
}

DownloadsTab::~DownloadsTab()
{
    download_flow::unlisten(this->listener);
}

void DownloadsTab::refresh()
{
    download_flow::show_pill();
    this->update();
}

brls::View* DownloadsTab::create()
{
    return new DownloadsTab();
}

void DownloadsTab::set_hints()
{
    const auto& jobs = download_flow::jobs();
    const bool  any_paused = std::any_of(jobs.begin(), jobs.end(), [](const download::Job& j) { return j.state == download::State::Paused; });
    const bool  any_done = std::any_of(jobs.begin(), jobs.end(), [](const download::Job& j) { return j.state == download::State::Done; });
    const bool  any_active = download::pending(jobs) > (size_t)std::count_if(jobs.begin(), jobs.end(), [](const download::Job& j) {
                                return j.state == download::State::Paused;
                            });
    brls::View* root = this;
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

void DownloadsTab::update()
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
                brls::Application::giveFocus(keep_awake);
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
            // The game's banner behind its right half, once it came.
            auto* backdrop = new BannerBackdrop();
            backdrop->setPositionTop(0);
            backdrop->setPositionBottom(0);
            backdrop->setPositionRight(0);
            backdrop->setWidthPercentage(55);
            r.card->addView(backdrop);
            r.backdrop = backdrop;
            // The game's icon, on the well until (and unless) the cache has it.
            auto* cover = new brls::Box(brls::Axis::ROW);
            cover->setWidth(ICON);
            cover->setHeight(ICON);
            cover->setShrink(0);
            cover->setCornerRadius(cadran::kit::RADIUS_ICON);
            cover->setThemeColor("backgroundColor", "cadran/well");
            cover->setAlignItems(brls::AlignItems::CENTER);
            cover->setJustifyContent(brls::JustifyContent::CENTER);
            cover->setMarginRight(20);
            cover->addView(new CoverPlaceholder(ICON, 36, cadran::kit::RADIUS_ICON));
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
            // The cache's (a cover shown before), else the title database's.
            const uint64_t game_id = g.jobs.empty() ? 0 : jobs[g.jobs.front()].game_id;
            auto           shown = ui::guarded(this->alive, [this, at, gen](const library_icons::Picture& p) {
                if (gen != this->generation || at >= this->rows.size()) return;
                Row& row = this->rows[at];
                if (!library_icons::show(row.icon, p)) return;
                row.icon->setVisibility(brls::Visibility::VISIBLE);
                row.icon->getParent()->getChildren()[0]->setVisibility(brls::Visibility::GONE);
            });
            library_icons::cached(g.icon_url, [shown, game_id](const library_icons::Picture& p) {
                if (p || !game_id) shown(p);
                else library_icons::local(game_id, shown);
            });
            if (game_id && config::get().nlib_api)
                library_icons::picture(titledb::nlib_banner_url(game_id), 854, 480,
                                       ui::guarded(this->alive, [this, at, gen](const library_icons::Picture& p) {
                                           if (p && gen == this->generation && at < this->rows.size())
                                               this->rows[at].backdrop->set_picture(p->rgba.data(), p->width, p->height);
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

void DownloadsTab::fill(Row& row)
{
    const auto& jobs = download_flow::jobs();
    download::Group g;
    uint64_t        size = 0;
    for (const std::string& job_id : row.ids)
        for (size_t i = 0; i < jobs.size(); i++)
            if (jobs[i].id == job_id) {
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
        // Nothing comes: said, before the transfer gives up and tries again.
        if (live.quiet >= download::QUIET_SHOWN) {
            text += " · " + brls::getStr("playshelf/downloads/state_quiet", short_duration(live.quiet));
            color = "cadran/amber";
        }
        break;
    case download::State::Waiting:
        text = "playshelf/downloads/state_waiting"_i18n + " · " + packages;
        // What is fetched ahead of it.
        for (size_t i : g.jobs)
            if (const auto a = download_flow::ahead_of(jobs[i].id)) {
                text += " · " + (a->first >= a->second ? "playshelf/downloads/ahead_done"_i18n
                                                        : brls::getStr("playshelf/downloads/ahead", a->first * 100 / a->second));
                break;
            }
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
        color = "cadran/coral";
        for (size_t i : g.jobs)
            if (jobs[i].state == download::State::Failed && text.empty()) {
                const std::string what = jobs[i].label.empty() ? jobs[i].name : jobs[i].label;
                text = what + " · " + cause_text(jobs[i]);
                // Tried again by itself: not the user's call yet.
                if (download_flow::retry_in(jobs[i]) >= 0) color = "cadran/amber";
            }
        break;
    }
    if (size && download::group_state(g, jobs) != download::State::Running) text += " · " + library_flow::size_text(size);
    row.state->setThemeColor("textColor", color);
    ui::set_text(row.state, text);
}

void DownloadsTab::open_game(const std::vector<std::string>& ids)
{
    std::vector<const download::Job*> pkgs;
    for (const std::string& job_id : ids)
        if (const download::Job* j = job_of(job_id)) pkgs.push_back(j);
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
            for (const std::string& job_id : ids) download_flow::pause(job_id);
        }, "pause");
    if (stopped)
        choices.emplace_back("playshelf/downloads/resume_game"_i18n, [ids]() {
            for (const std::string& job_id : ids) download_flow::resume(job_id);
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
        const std::string job_id = j->id;
        choices.emplace_back(j->label.empty() ? j->name : j->label,
                             ui::guarded(this->alive, [this, job_id]() { this->open_job(job_id); }), "file", state,
                             "playshelf/downloads/group_packages"_i18n, tone);
    }
    const std::string game = pkgs[0]->game.empty() ? pkgs[0]->name : pkgs[0]->game;
    choices.emplace_back("playshelf/downloads/remove_game"_i18n, [ids, game]() {
        ui::confirm_danger(brls::getStr("playshelf/downloads/remove_game_body", game), "playshelf/downloads/remove"_i18n,
                           [ids]() {
                               for (const std::string& job_id : ids) download_flow::cancel(job_id);
                           });
    }, "trash", "", "", Choice::Tone::DANGER);
    uint64_t size = 0;
    for (const download::Job* j : pkgs) size += j->size;
    ui::pick_actions(game, choices, size ? library_flow::size_text(size) : "");
}

void DownloadsTab::open_job(const std::string& job_id)
{
    const download::Job* j = job_of(job_id);
    if (!j) return;
    using Choice = ui::Choice;
    std::vector<Choice> choices;
    switch (j->state) {
    case download::State::Waiting:
    case download::State::Running:
        choices.emplace_back("playshelf/downloads/pause"_i18n, [job_id]() { download_flow::pause(job_id); }, "pause");
        break;
    case download::State::Paused:
        choices.emplace_back("playshelf/downloads/resume"_i18n, [job_id]() { download_flow::resume(job_id); }, "download");
        break;
    case download::State::Failed:
        for (download::Remedy r : download_flow::remedies(*j)) switch (r) {
            case download::Remedy::Retry:
                choices.emplace_back(download_flow::retry_in(*j) >= 0 ? "playshelf/downloads/retry_now"_i18n : "playshelf/downloads/retry"_i18n,
                                     [job_id]() { download_flow::resume(job_id); }, "refresh");
                break;
            case download::Remedy::OtherAddress:
                choices.emplace_back("playshelf/downloads/other_address"_i18n, [job_id]() { download_flow::use_other_address(job_id); },
                                     "globe");
                break;
            case download::Remedy::EditServer:
                choices.emplace_back("playshelf/downloads/edit_server"_i18n, [job_id]() {
                    const download::Job* job = job_of(job_id);
                    const auto           s = job ? download_flow::server_of(*job) : std::nullopt;
                    // Saved: the job tried again with what changed.
                    if (s) library_flow::edit(*s, [job_id](const std::string& saved) {
                        if (!saved.empty()) download_flow::resume(job_id);
                    });
                }, "server");
                break;
            case download::Remedy::FindAgain:
                choices.emplace_back("playshelf/downloads/find_again"_i18n, [job_id]() { download_flow::find_again(job_id); }, "eye");
                break;
            case download::Remedy::InstallAnyway:
                choices.emplace_back("playshelf/downloads/install_anyway"_i18n, [job_id]() { download_flow::accept_unsigned(job_id); },
                                     "alert", "", "", Choice::Tone::WARN);
                break;
            }
        break;
    case download::State::Done:
        break;
    }
    const bool        done = j->state == download::State::Done;
    const std::string name = j->name;
    choices.emplace_back(done ? "playshelf/downloads/forget"_i18n : "playshelf/downloads/remove"_i18n,
                         [job_id, name, done]() {
                             if (done) {
                                 download_flow::cancel(job_id);
                                 return;
                             }
                             ui::confirm_danger(brls::getStr("playshelf/downloads/remove_body", name),
                                                "playshelf/downloads/remove"_i18n, [job_id]() { download_flow::cancel(job_id); });
                         },
                         "trash", "", "", done ? Choice::Tone::PLAIN : Choice::Tone::DANGER);
    // A failure: what it means, and the error itself.
    std::string subtitle;
    if (j->state == download::State::Failed) {
        const download::Cause c = download::cause_of(j->error, j->unsigned_content);
        subtitle = c == download::Cause::Unsigned ? "playshelf/downloads/unsigned_note"_i18n
                 : c == download::Cause::Other    ? j->error
                                                  : brls::getStr(std::string("playshelf/downloads/help/") + download::cause_name(c));
        if (c != download::Cause::Other && c != download::Cause::Unsigned && !j->error.empty()) subtitle += "\n" + j->error;
    }
    ui::pick_actions(name, choices, subtitle);
}
