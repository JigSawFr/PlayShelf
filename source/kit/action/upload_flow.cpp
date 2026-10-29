// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/upload_flow.hpp"

#include <aurore.hpp>
#include <ctime>
#include <fmt/format.h>
#include <memory>
#include <vector>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/util/text.hpp>

#include "action/github_login_flow.hpp"
#include "app.hpp"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/diagnostics.hpp"
#include "util/github_auth.hpp"
#include "util/log_files.hpp"
#include "util/log_upload.hpp"
#include "util/paths.hpp"
#include "util/session_log.hpp"

using namespace aurore::literals;

namespace upload_flow
{

namespace
{
bool s_sending = false;      // one upload at a time
bool s_collecting = false;   // the files of a report being read

// The parts, shared by the dialogs and the tasks they lead to: a report with
// its files is a few hundred KB, read once and never copied after.
using Parts = std::shared_ptr<const std::vector<log_upload::Part>>;

// What goes: a diagnostic report (its link with the bug-report form, kept in
// the notifications) or a list (its link alone).
struct Kind
{
    bool        report = true;
    std::string what = "diagnostic report";   // the gist's description, in English
};

// Keeps the link in a file of its own, logs/uploads/upload_<date>.url
// (util/log_files.hpp): an Internet shortcut, so one can be found again after
// the dialog is closed, and opened from a computer.
void remember(const log_upload::Result& r)
{
    log_files::save(paths::uploads_dir(), log_files::UPLOAD, log_files::shortcut(r.url, "", ui::now_stamp()),
                    std::time(nullptr));
}

void show_link(const log_upload::Result& r, bool report)
{
    if (!report) {
        aurore::Box* code = cadran::kit::qr(r.url, 220, "playshelf/upload/qr_list"_i18n, log_upload::link_caption(r.url), 300);
        code->setAlignSelf(aurore::AlignSelf::CENTER);
        auto* dialog = ui::dialog("playshelf/upload/done_list"_i18n, code);
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return;
    }
    SysInfo si;
    sysinfo_get(&si);
    const std::string fw = ui::hos_text(si.hos_version);
    log_upload::IssueFields fields;
    fields.version = app::version();
    fields.firmware = fw;
    if (si.ams_valid) fields.atmosphere = fmt::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro);
    // The menu it was started from is not known: the reporter adds it.
    fields.launcher = si.applet_mode ? "from the Album (applet)" : "over a game (title override)";
    const SysStorage storage = sysinfo_storage(&si);
    if (const char* name = diagnostic::storage_name(storage)) fields.storage = name;
    const std::string issue = log_upload::issue_url(app::repo_url(), r.url, fields);

    // The link once, under its code: the text says which code is for whom,
    // where the link is found again and how the report is deleted.
    auto* row = new aurore::Box(aurore::Axis::ROW);
    row->setJustifyContent(aurore::JustifyContent::SPACE_EVENLY);
    row->addView(cadran::kit::qr(r.url, 200, "playshelf/upload/qr_report"_i18n, log_upload::link_caption(r.url), 300));
    row->addView(cadran::kit::qr(issue, 200, "playshelf/upload/qr_issue"_i18n, "playshelf/upload/qr_issue_note"_i18n, 300));
    auto* dialog = ui::dialog("playshelf/upload/done"_i18n, row);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

// To a secret gist in the GitHub account PlayShelf is signed in to.
void send_gist(const Parts& parts, const Kind& kind)
{
    // Too large (a long history): the start, with the report, is what matters.
    bool cut = false;
    const Parts files = std::make_shared<const std::vector<log_upload::Part>>(
        log_upload::gist_files(*parts, log_upload::GIST_MAX_BYTES, &cut));
    std::string list;
    for (const auto& p : *parts) list += "\n• " + p.name;
    if (cut) list += "\n" + "playshelf/upload/partial"_i18n;
    const int kb = (int)((log_upload::total_bytes(*files) + 1023) / 1024);
    std::string body = aurore::getStr("playshelf/upload/confirm", "playshelf/upload/where_gist"_i18n, kb) + "\n" +
                       list + "\n\n" + "playshelf/upload/confirm_gist"_i18n;
    // Its link goes into a bug report, which anyone can read.
    if (kind.report) body += " " + "playshelf/upload/confirm_public"_i18n;
    const std::string token = github_auth::token();
    ui::confirm(body, "playshelf/upload/send"_i18n, [parts, files, token, kind]() {
        if (s_sending) return;
        s_sending = true;
        ui::notify("playshelf/upload/sending"_i18n);
        ui::in_background("gist upload", [parts, files, token, kind]() {
            const log_upload::Result r = log_upload::upload_gist(*files, token, app::version(), kind.what);
            aurore::sync([parts, r, kind]() {
                s_sending = false;
                if (r.relogin) {
                    // The sign-in does not allow gists (or was revoked, and
                    // then forgotten): signing in again replaces it, then the
                    // same files go. A 403 or 404 keeps it (a 403 can also be
                    // a rate limit): the GitHub account cell signs out.
                    const std::string where = "playshelf/tabs/about"_i18n + " › " + "playshelf/prefs/help"_i18n +
                                              " › " + "playshelf/github/account"_i18n;
                    ui::confirm(aurore::getStr("playshelf/upload/relogin", where), "playshelf/upload/sign_in"_i18n,
                                [parts, kind]() {
                                    github_login_flow::sign_in([parts, kind]() { send_gist(parts, kind); });
                                });
                    return;
                }
                if (!r.ok) {
                    ui::error(aurore::getStr(kind.report ? "playshelf/upload/failed" : "playshelf/upload/failed_list",
                                          r.error));
                    return;
                }
                remember(r);
                show_link(r, kind.report);
                // A report's link stays in the app's notifications, for later.
                if (!kind.report || !ui::app_hooks::get().notice_set) return;
                ui::app_hooks::get().notice_set("upload", "playshelf/notices/upload"_i18n, text::short_url(r.url),
                                            "playshelf/notices/upload_show"_i18n, [r]() { show_link(r, true); });
            });
        });
    });
}

// Signed out of GitHub: signing in comes first, then the report goes.
void send(const Parts& parts, const Kind& kind = Kind{})
{
    if (!github_auth::token().empty()) {
        send_gist(parts, kind);
        return;
    }
    ui::confirm(kind.report ? "playshelf/upload/needs_github"_i18n : "playshelf/upload/needs_github_list"_i18n,
                "playshelf/upload/sign_in"_i18n, [parts, kind]() {
                    github_login_flow::sign_in([parts, kind]() { send_gist(parts, kind); });
                });
}

Parts parts_of(std::vector<log_upload::Part> parts)
{
    return std::make_shared<const std::vector<log_upload::Part>>(std::move(parts));
}

// The report, the latest crashes (PlayShelf's, then Atmosphère's), the last
// runs' session logs (before the debug files, so a cut takes the history
// first) and the debug files. Read off the UI thread: the files are on the
// SD card (only the part of each that is sent is read), then the sending
// goes on, on the UI thread.
void with_debug_files(const std::string& report)
{
    if (s_collecting) return;
    s_collecting = true;
    const std::string name = "playshelf/upload/part_report"_i18n;
    ui::in_background("report files", [name, report]() {
        std::vector<log_upload::Part> parts{ { name, report } };
        session_log::flush();   // this run's lines still in memory, into its file
        ui::settle_writes();    // the files as they are once the writes asked for are done
        for (auto& f : log_upload::crash_files()) parts.push_back(std::move(f));
        for (auto& f : log_upload::atmosphere_crashes(std::time(nullptr))) parts.push_back(std::move(f));
        for (auto& f : log_upload::session_files()) parts.push_back(std::move(f));
        for (auto& f : log_upload::debug_files()) parts.push_back(std::move(f));
        if (ui::quitting()) return;
        const Parts all = parts_of(std::move(parts));
        aurore::sync([all]() {
            s_collecting = false;
            send(all);
        });
    });
}
}   // namespace

void send_report(const std::string& report)
{
    with_debug_files(report);
}

void send_list(const std::string& name, const std::string& text, const std::string& what)
{
    Kind kind;
    kind.report = false;
    kind.what = what;
    send(parts_of({ { name, text } }), kind);
}

void choose()
{
    const std::vector<std::string> saved = log_upload::saved_reports();
    if (saved.empty()) {
        send_report(diagnostic::current_report());
        return;
    }
    std::vector<std::string> labels{ "playshelf/upload/current"_i18n };
    for (const auto& name : saved) labels.push_back(aurore::getStr("playshelf/upload/saved", name));
    ui::pick("playshelf/upload/pick"_i18n, labels, 0, [saved](int i) {
        if (i == 0) {
            send_report(diagnostic::current_report());
            return;
        }
        const std::string name = saved[(size_t)i - 1];
        std::string content;
        if (!paths::read_file(paths::reports_dir() + "/" + name, content)) {
            ui::error(aurore::getStr("playshelf/upload/failed", "cannot read " + name));
            return;
        }
        send(parts_of({ { "logs/reports/" + name, std::move(content) } }));
    }, false);
}

}   // namespace upload_flow
