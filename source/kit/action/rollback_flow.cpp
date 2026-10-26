// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/rollback_flow.hpp"

#include <borealis.hpp>
#include <cstdio>

#include "action/dev_build_flow.hpp"
#include "action/upload_flow.hpp"
#include "app.hpp"
#include "core/platform.h"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "util/launcher.hpp"
#include "util/migrations.hpp"
#include "util/paths.hpp"
#include "util/rollback.hpp"

using namespace brls::literals;

namespace rollback_flow
{

namespace
{
std::string label_file() { return paths::data_dir() + "/previous_build.json"; }
std::string check_file() { return paths::data_dir() + "/boot_check.json"; }
// Gone back from a build: its name, and `starts` 1 when its start did not
// finish (0: asked for). Read and removed by the build gone back to.
std::string back_file() { return paths::data_dir() + "/went_back.json"; }

// util/migrations.hpp's lines, in the log.
void log_migration(bool warning, const std::string& line)
{
    if (warning)
        brls::Logger::warning("rollback: {}", line);
    else
        brls::Logger::info("rollback: {}", line);
}

// The swap, the running build's name kept for the one now previous. romfs:/
// is read from the running .nro and holds it open: released around it.
bool swap(const std::string& nro, std::string* error, bool crashed)
{
    // The previous build may write the data in an earlier layout: the
    // migrations run again when this one is back (util/migrations.hpp).
    migrations::earlier_build_next(paths::migrations_file(), log_migration);
    platform_romfs_release();
    const bool ok = rollback::swap(nro, error);
    platform_romfs_mount();
    if (!ok) return false;
    rollback::write_label(label_file(), running_label());
    std::remove(check_file().c_str());
    rollback::Check back;
    back.label  = running_label();
    back.starts = crashed ? 1 : 0;
    rollback::write_check(back_file(), back);
    return true;
}
}   // namespace

bool at_start()
{
    const std::string nro = dev_build_flow::nro_file();
    if (nro.empty()) return false;
    // An install or a swap cut short by a power loss: finished (the build
    // replaced kept as the previous one) before it is looked for.
    rollback::recover(nro);
    // This start counts: it is over once the first screen is up (started()).
    rollback::Check check;
    const bool checked = rollback::read_check(check_file(), &check);
    const int before = check.starts;
    if (checked) {
        check.starts++;
        rollback::write_check(check_file(), check);
    }
    if (!rollback::offer_at_start(checked, before, paths::file_exists(rollback::previous_file(nro)))) return false;

    std::string previous;
    if (!rollback::read_label(label_file(), &previous)) previous = "the previous build";
    // Plain ASCII, in English: borealis (and the translations) are not up yet.
    const std::string text = "\n  PlayShelf " + running_label() +
                             "\n\n  Its last start did not finish (it may have crashed).\n\n  (A)  Go back to " +
                             previous + "\n  (B)  Start " + running_label() + "\n";
    brls::Logger::info("rollback: offered (crashed)");
    if (!platform_boot_choice(text.c_str())) return false;

    std::string err;
    if (!swap(nro, &err, true)) {
        platform_boot_choice(("\n  Could not go back: " + err + "\n\n  (A) / (B)  Start PlayShelf\n").c_str());
        return false;
    }
    brls::Logger::info("rollback: back to {}", previous);
    if (launcher::can_launch() && launcher::launch_nro(dev_build_flow::nro_sd_path())) return true;
    platform_boot_choice(("\n  " + previous + " is back in place.\n  Close PlayShelf and start it again.\n\n  (A) / (B)  Close\n").c_str());
    return true;
}

void started()
{
    rollback::Check check;
    if (rollback::read_check(check_file(), &check) && !check.label.empty()) {
        std::string previous;
        if (!rollback::read_label(label_file(), &previous)) previous = "?";
        if (ui::app_hooks::get().notice_set)
            ui::app_hooks::get().notice_set("installed", brls::getStr("playshelf/notices/installed", check.label),
                                        brls::getStr("playshelf/notices/installed_detail", previous), "", nullptr);
    }
    std::remove(check_file().c_str());
}

void note_went_back()
{
    rollback::Check back;
    if (!rollback::read_check(back_file(), &back)) return;
    std::remove(back_file().c_str());
    const std::string from = back.label.empty() ? "?" : back.label;
    const auto& h = ui::app_hooks::get();
    if (!h.notice_set) return;
    h.notice_set("went_back", "playshelf/notices/went_back"_i18n,
                 brls::getStr(back.starts ? "playshelf/notices/went_back_crash" : "playshelf/notices/went_back_detail",
                              from, running_label()),
                 back.starts ? "playshelf/notices/send_report"_i18n : "", []() {
                     if (ui::app_hooks::get().notice_clear) ui::app_hooks::get().notice_clear("went_back");
                     upload_flow::choose();
                 });
}

std::string running_label()
{
    // Plain ASCII: the start-up's text screen shows it too (at_start()).
    std::string label = app::version();
    if (app::pr() && !app::commit().empty()) label += " (#" + std::to_string(app::pr()) + ", " + app::commit() + ")";
    else if (!app::commit().empty()) label += " (" + app::commit() + ")";
    return label;
}

void installed(const std::string& installed_label)
{
    rollback::write_label(label_file(), running_label());
    rollback::Check check;
    check.label = installed_label;
    rollback::write_check(check_file(), check);
}

bool previous(std::string* label)
{
    const std::string nro = dev_build_flow::nro_file();
    if (nro.empty() || !paths::file_exists(rollback::previous_file(nro))) return false;
    if (!rollback::read_label(label_file(), label)) *label = "?";
    return true;
}

void go_back_now()
{
    std::string label, err;
    if (!previous(&label)) return;
    if (!swap(dev_build_flow::nro_file(), &err, false)) {
        ui::error(brls::getStr("playshelf/dev_build/back_failed", err));
        return;
    }
    if (launcher::can_launch() && launcher::launch_nro(dev_build_flow::nro_sd_path())) {
        brls::Application::quit();   // hbloader starts it as soon as this one exits
        return;
    }
    ui::info(brls::getStr("playshelf/dev_build/done_manual", label));
}

}   // namespace rollback_flow
