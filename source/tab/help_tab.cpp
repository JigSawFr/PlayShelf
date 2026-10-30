// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/help_tab.hpp"

#include <format>

#include <aurore/cadran/ui/cadran_list.hpp>

#include "action/cleanup_flow.hpp"
#include "action/dev_build_flow.hpp"
#include "action/frame_stats_flow.hpp"
#include "action/github_login_flow.hpp"
#include "action/install_flow.hpp"
#include "action/log_prune_flow.hpp"
#include "action/upload_flow.hpp"
#include "activity/diagnostic_activity.hpp"
#include "app.hpp"
#include "core/sysinfo.h"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/frame_stats.hpp"
#include "util/github_auth.hpp"
#include "util/paths.hpp"

using namespace aurore::literals;

namespace
{
// The limits of the reports and crash files kept (config logs_keep*).
std::string logs_keep_text(int n)
{
    return n == 0 ? "playshelf/tools/logs_keep_all"_i18n : aurore::getStr("playshelf/tools/logs_keep_n", n);
}
std::string logs_days_text(int n)
{
    return n == 0 ? "playshelf/tools/logs_days_never"_i18n : aurore::getStr("playshelf/tools/logs_days_n", n);
}
std::string logs_mb_text(int n)
{
    return n == 0 ? "playshelf/common/no_limit"_i18n : aurore::getStr("playshelf/dev/size_mb", n);
}

// A picker over `values` for `field`; the limits apply at once.
template <size_t N>
void pick_limit(const std::string& title, const int (&values)[N], int& field, std::string (*text)(int),
                std::function<void()> done)
{
    std::vector<std::string> labels;
    int selected = 0;
    for (size_t i = 0; i < N; i++) {
        labels.push_back(text(values[i]));
        if (values[i] == field) selected = (int)i;
    }
    ui::pick(title, labels, selected, [&values, &field, done](int i) {
        field = values[i];
        ui::save_config();
        log_prune_flow::run();
        done();
    });
}
}   // namespace

HelpTab::HelpTab()
    : TabBase("xml/tab/help.xml", false)
{
    export_note->setSingleLine(false);

    export_note->setText(aurore::getStr("playshelf/tools/export_note", paths::logs_dir()));

    export_cell->registerClickAction([](aurore::View*) {
        std::string err;
        std::string path = diagnostic::save(diagnostic::current_report(), &err);
        if (path.empty()) ui::error("playshelf/toast/diag_err"_i18n, err);
        else ui::notify(aurore::getStr("playshelf/toast/diag_saved", path));
        return true;
    });

    // Only reads and the network: also offered in read-only mode.
    upload_note->setSingleLine(false);
    upload->registerClickAction([](aurore::View*) {
        upload_flow::choose();
        return true;
    });
    // The account a report goes with: signing out takes it off the console.
    // Shown once signed in (Send a report online signs in without developer
    // mode), and to a developer, whose builds need it.
    github->registerClickAction([this](aurore::View*) {
        github_login_flow::open(this->refresher());
        return true;
    });

    // Only the SD card: also offered in read-only mode.
    logs_note->setSingleLine(false);
    logs_note->setText(aurore::getStr("playshelf/tools/logs_note", paths::logs_dir()));
    logs_keep->registerClickAction([this](aurore::View*) {
        pick_limit("playshelf/tools/logs_keep"_i18n, config::LOGS_KEEP, config::get().logs_keep, logs_keep_text,
                   this->refresher());
        return true;
    });
    logs_keep_days->registerClickAction([this](aurore::View*) {
        pick_limit("playshelf/tools/logs_keep_days"_i18n, config::LOGS_KEEP_DAYS, config::get().logs_keep_days,
                   logs_days_text, this->refresher());
        return true;
    });
    logs_keep_mb->registerClickAction([this](aurore::View*) {
        pick_limit("playshelf/tools/logs_keep_mb"_i18n, config::LOGS_KEEP_MB, config::get().logs_keep_mb, logs_mb_text,
                   this->refresher());
        return true;
    });

    // Developer tools.
    // Turning a developer switch off asks first: the switch goes back on
    // until it is confirmed. Turning one on does not ask.
    dev_mode->init("playshelf/dev/mode"_i18n, app::dev_mode(), [this](bool on) {
        if (on) {
            app::set_dev_mode(true, true);
            ui::on_mode_changed();
            return;
        }
        this->dev_mode->setOn(true, false);
        ui::confirm("playshelf/dev/mode_off_body"_i18n, "playshelf/dev/mode_off_confirm"_i18n, []() {
            app::set_dev_mode(false, true);
            ui::on_mode_changed();
        });
    });
    dev_read_only->init("playshelf/dev/read_only"_i18n, app::read_only(), [this](bool on) {
        if (on) {
            app::set_read_only(true);
            ui::on_mode_changed();
            return;
        }
        this->dev_read_only->setOn(true, false);
        ui::confirm("playshelf/dev/read_only_off_body"_i18n, "playshelf/dev/read_only_off_confirm"_i18n, []() {
            app::set_read_only(false);
            ui::on_mode_changed();
        });
    });
    dev_frame_stats->init("playshelf/dev/frame_stats"_i18n, config::get().frame_stats, [this](bool on) {
        config::get().frame_stats = on;
        ui::save_config();
        frame_stats_flow::apply();
        if (on) ui::notify(aurore::getStr("playshelf/dev/frame_stats_on", frame_stats::path()));
        this->update_frame_stats();
    });
    dev_frame_stats_last->setSingleLine(false);
    // Cadran's background glow, off to weigh it with the frame statistics.
    dev_glow->init("playshelf/dev/glow"_i18n, config::get().look.glow, [](bool on) {
        config::get().look.glow = on;
        ui::save_config();
    });
    this->stats_timer.setPeriod(1000);
    this->stats_timer.setCallback([this]() { this->update_frame_stats(); });
    dev_clear_cache->registerClickAction([](aurore::View*) {
        cleanup_flow::clear_cache();
        return true;
    });
    dev_clear_logs->registerClickAction([](aurore::View*) {
        cleanup_flow::clear_logs();
        return true;
    });
    dev_report->registerClickAction([](aurore::View*) {
        aurore::Application::pushActivity(new DiagnosticActivity());
        return true;
    });
    dev_build->registerClickAction([](aurore::View*) {
        dev_build_flow::open();
        return true;
    });
    dev_install->registerClickAction([](aurore::View*) {
        install_flow::from_url();
        return true;
    });
    // A report as a tile, an icon per line (the screen's
    // skin would dress it without).
    cadran::List list(this, true);
    list.tiles({ { "hp_upload", "upload" } }, false);
    list.apply({ { "hp_export", "file" }, { "hp_github", "user" }, { "hp_mode", "gamepad" }, { "hp_data", "folder" },
                 { "hp_logs_keep", "folder" }, { "hp_logs_keep_days", "calendar" },
                 { "hp_logs_keep_mb", "save" }, { "hp_dev_mode", "code" }, { "hp_dev_read_only", "eye" },
                 { "hp_dev_frame_stats", "activity" }, { "hp_dev_glow", "sparkle" },
                 { "hp_dev_clear_cache", "trash" }, { "hp_dev_clear_logs", "trash" },
                 { "hp_dev_report", "file" }, { "hp_dev_build", "download" },
                 { "hp_dev_install", "save" } });
}

void HelpTab::refresh()
{
    const auto& cfg = config::get();
    // How PlayShelf was opened and where it keeps its files (About before).
    SysInfo si;
    sysinfo_get(&si);
    mode->setDetailText(si.applet_mode ? "playshelf/tools/mode_applet"_i18n : "playshelf/tools/mode_app"_i18n);
    data->setDetailText(paths::data_dir());
    logs_keep->setDetailText(logs_keep_text(cfg.logs_keep));
    logs_keep_days->setDetailText(logs_days_text(cfg.logs_keep_days));
    logs_keep_mb->setDetailText(logs_mb_text(cfg.logs_keep_mb));
    const bool ro  = app::read_only();
    const bool dev = app::dev_mode();
    dev_mode->setOn(dev, false);
    dev_read_only->setOn(ro, false);
    dev_frame_stats->setOn(cfg.frame_stats, false);
    dev_glow->setOn(cfg.look.glow, false);
    this->update_frame_stats();
    const bool signed_in = !github_auth::token().empty();
    ui::set_visible_all({ { github.getView(), dev || signed_in },
                          { dev_header.getView(), dev },
                          { dev_mode.getView(), dev },
                          { dev_read_only.getView(), dev },
                          { dev_frame_stats.getView(), dev },
                          { dev_frame_stats_note.getView(), dev },
                          { dev_frame_stats_last.getView(), dev && frame_stats_flow::running() },
                          { dev_glow.getView(), dev },
                          { dev_clear_cache.getView(), dev },
                          { dev_clear_logs.getView(), dev },
                          { dev_report.getView(), dev },
                          { dev_build.getView(), dev } });
    github->setDetailText(signed_in ? "playshelf/github/signed_in_short"_i18n : "playshelf/github/not_signed_in"_i18n);
}

// The last second's line under the switch, while the statistics run (hidden
// otherwise); refreshed every second while this tab is on screen.
void HelpTab::update_frame_stats()
{
    const bool on = app::dev_mode() && frame_stats_flow::running();
    if (on && !this->stats_timer.isRunning()) this->stats_timer.start();
    else if (!on && this->stats_timer.isRunning()) this->stats_timer.stop();
    const std::string last = on ? frame_stats_flow::last() : std::string();
    dev_frame_stats_last->setText(last.empty() ? "playshelf/dev/frame_stats_wait"_i18n : last);
    ui::set_visible(dev_frame_stats_last.getView(), on);
}
