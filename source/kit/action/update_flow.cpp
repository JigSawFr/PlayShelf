// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/update_flow.hpp"

#include "action/dev_build_flow.hpp"
#include "activity/update_activity.hpp"
#include "app.hpp"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/date.hpp"
#include "util/launcher.hpp"

using namespace brls::literals;

namespace update_flow
{

const std::vector<std::string> METHODS = { "inplace", "auto", "sphaira", "appstore", "manual" };

namespace
{
// The start-up screen waits for what is in front of the main screen (first
// steps, What's new, a question) to close: tried every 2 s, for a minute.
constexpr long WAIT_STEP_MS = 2000;
constexpr int  WAIT_STEPS   = 30;

bool  s_checking = false;   // the release's details or the store's version asked
bool  s_check_running = false;   // the update check: one at a time, whoever started it
bool  s_checked = false;    // s_offer is this session's, from the server
Offer s_offer;              // the last check's, for update_now()
// Those waiting for the check under way: each told its result.
std::vector<std::function<void(const Offer&)>> s_waiting;

bool busy()
{
    return s_checking || s_check_running;
}

std::string store_name(launcher::Store store)
{
    switch (store) {
        case launcher::Store::Sphaira:  return "sphaira";
        case launcher::Store::AppStore: return "playshelf/update/appstore"_i18n;
        default:                        return "";
    }
}

bool newer(const update::Result& r)
{
    return r.verdict == update::Verdict::UpdateSupports || r.verdict == update::Verdict::UpdateNoSupport;
}

update::Snooze snooze()
{
    const auto& cfg = config::get();
    return update::Snooze{ cfg.update_snooze_version, cfg.update_snooze_until };
}

void show_result(const Offer& o)
{
    switch (o.result.verdict) {
        case update::Verdict::UpdateSupports:
        case update::Verdict::UpdateNoSupport:
            UpdateActivity::show(o);
            break;
        case update::Verdict::UpToDate:
            ui::info(brls::getStr("playshelf/update/up_to_date", app::version()));
            break;
        case update::Verdict::NoRelease:
            ui::info(brls::getStr("playshelf/update/no_release", app::version()));
            break;
        default:
            ui::error(brls::getStr("playshelf/update/failed", o.result.error));
            break;
    }
}

// The notifications' entry for a newer version `latest` ("" or not
// newer: none). Information: Later on the update screen does not hide it.
void note_latest(const std::string& latest)
{
    const auto& h = ui::app_hooks::get();
    if (latest.empty() || update::compare_versions(latest, app::version()) <= 0) {
        if (h.notice_clear) h.notice_clear("update");
        return;
    }
    if (h.notice_set)
        h.notice_set("update", brls::getStr("playshelf/update/headline", latest),
                     brls::getStr("playshelf/update/running", app::version()), "playshelf/update/update"_i18n,
                     []() { update_now(); });
}

// Remembers the day of a check that reached the server, and what it found.
void record_check(const update::Result& r)
{
    if (r.verdict == update::Verdict::Unknown) return;
    note_latest(r.verdict == update::Verdict::NoRelease ? "" : r.latest.version);
    auto& cfg = config::get();
    cfg.update_checked = ui::today_date();
    cfg.update_latest  = r.verdict == update::Verdict::NoRelease ? "" : r.latest.version;
    ui::save_config();
}

// Asks the server (or joins the request under way), then tells `done`.
void run_check(std::function<void(const Offer&)> done)
{
    s_waiting.push_back(std::move(done));
    if (s_check_running) return;
    s_check_running = true;
    const uint32_t fw = sysinfo_hos_version();
    ui::in_background("update check", [fw]() {
        Offer o;
        o.result = update::check(fw);
        // The newer release's file and notes, for the update screen and an
        // update in place (shown without its notes when they cannot be read).
        if (newer(o.result)) {
            std::string err;
            o.has_release = dev_builds::fetch_release(&o.release, &err) &&
                            update::compare_versions(o.release.version, o.result.latest.version) == 0;
            if (!o.has_release && !err.empty()) brls::Logger::warning("update: no release details ({})", err);
        }
        brls::sync([o]() {
            s_check_running = false;
            s_offer = o;
            s_checked = o.result.verdict != update::Verdict::Unknown;
            record_check(o.result);
            std::vector<std::function<void(const Offer&)>> waiting;
            waiting.swap(s_waiting);
            for (const auto& tell : waiting) tell(o);
        });
    });
}

// Shows the start-up screen once `main` is on top, waiting for what is in
// front of it to close.
void show_when_free(brls::Activity* main, const Offer& o, int steps_left)
{
    if (ui::on_top(main)) {
        // Not after another screen of its own this start (First steps): the next.
        if (ui::claim_start_screen("the update screen")) UpdateActivity::show(o);
        return;
    }
    if (steps_left <= 0) return;   // still busy: the next start-up
    brls::delay(WAIT_STEP_MS, [main, o, steps_left]() { show_when_free(main, o, steps_left - 1); });
}

void install_in_place(const dev_builds::Build& b)
{
    ui::confirm(brls::getStr("playshelf/update/inplace_body", b.version), "playshelf/update/inplace_confirm"_i18n,
                [b]() { dev_build_flow::install_now(b); });
}

void update_in_place()
{
    if (s_offer.has_release && newer(s_offer.result)) {
        install_in_place(s_offer.release);
        return;
    }
    // The details could not be read during the check: fetched now.
    if (busy()) return;
    s_checking = true;
    ui::notify("playshelf/update/checking"_i18n);
    ui::in_background("update release", []() {
        dev_builds::Build b;
        std::string err;
        const bool ok = dev_builds::fetch_release(&b, &err);
        brls::sync([ok, b, err]() {
            s_checking = false;
            if (!ok) ui::error(brls::getStr("playshelf/update/failed", err));
            else if (update::compare_versions(b.version, app::version()) <= 0)
                ui::info(brls::getStr("playshelf/update/up_to_date", app::version()));
            else install_in_place(b);
        });
    });
}

void hand_over(const launcher::Target& target, const std::string& manual)
{
    const std::string name = store_name(target.store);
    ui::confirm(brls::getStr("playshelf/update/open_body", name), brls::getStr("playshelf/update/open_confirm", name),
                [target, manual]() {
                    const auto& may = ui::app_hooks::get().may_replace_self;
                    if (may && !may()) return;
                    if (launcher::launch(target)) brls::Application::quit();
                    else ui::info(manual);
                });
}

// Hands over to sphaira or the App Store; explains how to update by hand
// when neither can be opened. When the App Store installed PlayShelf, its
// catalogue is asked first: it publishes a release after a review, days
// after GitHub, and a store that still has the version installed would show
// nothing to update. Then PlayShelf offers to update itself instead.
void open_store()
{
    const launcher::Target target = launcher::find(config::get().update_via);
    const std::string manual = brls::getStr("playshelf/update/manual_body", std::string(app::repo_url()) + "/releases");
    if (target.store == launcher::Store::None || !launcher::can_launch()) {
        ui::info(manual);
        return;
    }
    const std::string latest = config::get().update_latest;
    if (latest.empty() || !launcher::installed_by_store() || busy()) {
        hand_over(target, manual);
        return;
    }
    s_checking = true;
    ui::in_background("store version", [target, manual, latest]() {
        std::string err;
        const std::string listed = launcher::store_version(&err);
        brls::sync([target, manual, latest, listed, err]() {
            s_checking = false;
            // Not known (offline, not listed yet): the store, as asked.
            if (listed.empty() || update::compare_versions(listed, latest) >= 0) {
                if (listed.empty()) brls::Logger::warning("update: the store's version unknown ({})", err);
                hand_over(target, manual);
                return;
            }
            ui::confirm(brls::getStr("playshelf/update/store_behind", store_name(launcher::Store::AppStore), listed, latest),
                        "playshelf/update/store_behind_confirm"_i18n, []() { update_in_place(); });
        });
    });
}
}   // namespace

void check_now()
{
    // Pressing again while it runs says so, instead of starting another
    // request and stacking a second result dialog.
    ui::notify("playshelf/update/checking"_i18n);
    if (busy()) return;
    run_check([](const Offer& o) { show_result(o); });
}

void check_once(std::function<void(const Offer&)> done)
{
    if (s_checked && !s_check_running) {
        // Next frame, as after a request.
        brls::sync([done]() { done(s_offer); });
        return;
    }
    run_check(std::move(done));
}

void at_start(brls::Activity* main)
{
    const auto& cfg = config::get();
    const std::string today = ui::today_date();
    const bool pending = update::offer_at_start(cfg.update_latest, app::version(), snooze(), today);
    const bool due = cfg.update_daily && cfg.update_checked != today;
    note_latest(cfg.update_latest);   // what the last check found, until the next one
    if (s_checking || !(pending || due)) return;
    // Quietly: only a newer version, not postponed, is worth a word.
    check_once([main, today](const Offer& o) {
        if (newer(o.result) && update::offer_at_start(o.result.latest.version, app::version(), snooze(), today))
            show_when_free(main, o, WAIT_STEPS);
    });
}

void update_now()
{
    const std::string& via = config::get().update_via;
    if (via.empty()) choose_method([]() { update_now(); }, true);
    else if (via == "inplace") update_in_place();
    else open_store();
}

void choose_method(std::function<void()> then, bool first)
{
    // Installed by the App Store: updated where it came from, so the store's
    // record follows (sphaira, else the App Store: both read its catalogue).
    const std::string recommended = launcher::installed_by_store() ? "auto" : METHODS[0];
    std::vector<std::string> labels;
    for (const std::string& m : METHODS) {
        std::string label = brls::getStr("playshelf/tools/update_via_values/" + m);
        if (first && m == recommended) label = brls::getStr("playshelf/update/recommended", label);
        labels.push_back(label);
    }
    int selected = 0;
    for (size_t i = 0; i < METHODS.size(); i++)
        if (METHODS[i] == (config::get().update_via.empty() ? recommended : config::get().update_via)) selected = (int)i;
    ui::pick(first ? "playshelf/update/method_title"_i18n : "playshelf/tools/update_via"_i18n, labels, selected,
             [then](int i) {
                 config::get().update_via = METHODS[i];
                 ui::save_config();
                 if (then) then();
             });
}

std::string method_text()
{
    const std::string& via = config::get().update_via;
    return via.empty() ? "playshelf/tools/update_via_values/unset"_i18n
                       : brls::getStr("playshelf/tools/update_via_values/" + via);
}

void postpone(const std::string& version, std::function<void()> done)
{
    static const int DAYS[] = { 1, 7, update::SNOOZE_MAX_DAYS };
    const std::vector<std::string> labels = { "playshelf/update/later_day"_i18n, "playshelf/update/later_week"_i18n,
                                              brls::getStr("playshelf/update/later_days", update::SNOOZE_MAX_DAYS) };
    ui::pick("playshelf/update/later_title"_i18n, labels, 0, [version, done](int i) {
        const std::string until = date::add_days(ui::today_date(), DAYS[i]);
        if (until.empty()) return;   // no date on the console clock: nothing to count from
        auto& cfg = config::get();
        cfg.update_snooze_version = version;
        cfg.update_snooze_until   = until;
        ui::save_config(brls::getStr("playshelf/update/later_done", until));
        if (done) done();
    }, false);
}

std::string status_text(const update::Result& r, const std::string& firmware)
{
    switch (r.verdict) {
        case update::Verdict::UpdateSupports:
            return brls::getStr("playshelf/fw_gate/status_supports", r.latest.version, firmware);
        case update::Verdict::UpdateNoSupport:
            return brls::getStr("playshelf/fw_gate/status_no_support", r.latest.version, firmware);
        case update::Verdict::UpToDate:
        case update::Verdict::NoRelease:
            return brls::getStr("playshelf/fw_gate/status_latest", app::version());
        default:
            return brls::getStr("playshelf/fw_gate/status_unknown", r.error);
    }
}

std::string update_label()
{
    const std::string& via = config::get().update_via;
    if (via.empty()) return "playshelf/update/update"_i18n;
    if (via == "inplace") return "playshelf/update/update_now"_i18n;
    const launcher::Target target = launcher::find(via);
    if (target.store == launcher::Store::None || !launcher::can_launch()) return "playshelf/update/update_manual"_i18n;
    return brls::getStr("playshelf/update/update_with", store_name(target.store));
}

}   // namespace update_flow
