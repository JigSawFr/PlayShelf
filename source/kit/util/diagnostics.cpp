// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/diagnostics.hpp"

#include <ctime>
#include <format>
#include <sys/statvfs.h>
#include <utility>
#include <vector>

#include "app.hpp"
#include "app_id.h"
#include "util/log_files.hpp"
#include "util/patches.hpp"
#include "util/paths.hpp"
#include "util/perf.hpp"
#include "util/session_log.hpp"
#include "util/startup.h"
#include "core/platform.h"
#include "core/sysinfo.h"

namespace diagnostic
{

namespace
{
std::vector<std::function<std::string()>> s_sections;
std::vector<std::function<std::string()>> s_header_lines;

// This run, the one before, the memory, the SD card and the crash files kept.
std::string logs_section()
{
    std::string out = "=== logs ===\n";
    const std::string current = session_log::current_name();
    out += std::format("session     : {}, {} s since the start\n",
                       current.empty() ? std::string("no session file") : "logs/sessions/" + current,
                       session_log::uptime_s());

    // The run before this one, and how it ended.
    std::string before = "none kept";
    for (const std::string& name : log_files::list(paths::sessions_dir(), log_files::SESSION)) {
        if (name == current) continue;
        std::string text;   // its end line: the end of the file is enough
        if (!session_log::tail_of(paths::sessions_dir() + "/" + name, &text)) {
            before = name + ": cannot be read";
            break;
        }
        const std::string reason = session_log::end_reason(text);
        before = name + ": " + (reason.empty() ? std::string("did not end (crash, hang or power off)") : reason);
        break;
    }
    out += "last run    : " + before + "\n";

    uint64_t used = 0, total = 0;
    out += "memory      : " +
           (platform_memory(&used, &total) ? std::format("{} allocated, of {} for the process", bytes(used), bytes(total)) : std::string("unknown")) +
           "\n";
    struct statvfs fs;
    if (statvfs(paths::sd_root().c_str(), &fs) == 0 && fs.f_blocks > 0)
        out += std::format("sd card     : {} free of {}\n", bytes((uint64_t)fs.f_bavail * fs.f_frsize),
                           bytes((uint64_t)fs.f_blocks * fs.f_frsize));
    else
        out += "sd card     : unknown\n";

    // The newest crash files, with their first line (what happened).
    const std::vector<std::string> crashes = log_files::list(paths::crashes_dir(), log_files::CRASH);
    out += std::format("crash files : {}{}\n", crashes.size(), crashes.empty() ? "" : " (newest first)");
    for (size_t i = 0; i < crashes.size() && i < 5; i++) {
        std::string text;
        paths::read_file(paths::crashes_dir() + "/" + crashes[i], text);
        out += "  " + crashes[i] + "\n    " + first_line(text) + "\n";
    }
    return out + "\n";
}
}   // namespace

void add_section(std::function<std::string()> section) { s_sections.push_back(std::move(section)); }
void add_header_line(std::function<std::string()> line) { s_header_lines.push_back(std::move(line)); }

std::string current_report()
{
    SysInfo si;
    sysinfo_get(&si);
    char fw[16];
    sysinfo_version_string(si.hos_version, fw, sizeof(fw));

    // Game patches: sys-patch state and sigpatch files (no personal data).
    const std::string patches_text = patch_line(patches::detect(paths::sd_root(), fw, si.emummc));

    // The serial number itself is never written: reports are attached to
    // public bug reports.
    const char* storage = storage_name(sysinfo_storage(&si));
    std::string out = std::format(
        "=== " APP_NAME " diagnostic ===\n"
        "app version : {}{}\n"
        "modes       : read_only={} dev={} platform={}\n"
        "firmware    : {}\n"
        "atmosphere  : {} ({})\n"
        "launch mode : {}\n"
        "storage     : {}\n"
        "prodinfo    : {}\n"
        "game patches: {}\n",
        app::version(), app::commit().empty() ? std::string() : " (commit " + app::commit() + ")",
        app::read_only() ? 1 : 0, app::dev_mode() ? 1 : 0,
#ifdef __SWITCH__
        "switch",
#else
        "desktop-simulation",
#endif
        fw,
        si.ams_valid ? std::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro) : std::string("unknown"),
        si.is_atmosphere ? "detected" : "not detected",
        si.applet_mode ? "applet (album)" : "application (over a game)",
        std::format("{} (data: nand/{})", storage ? storage : "unknown", paths::nand_key()),
        !si.blank_valid ? "unknown" : (si.blank ? "blanked" : "not blanked"),
        patches_text);
    for (const auto& line : s_header_lines) out += line();
    out += "\n";

    // How long this run took to start, step by step (util/startup.h).
    std::vector<char> buf(4096);
    startup_report(buf.data(), buf.size());
    out += buf.data();
    out += "\n";
    // How long the slow work of the screens took in this run (util/perf.hpp).
    out += perf::report() + "\n";
    for (const auto& section : s_sections) out += section();
    out += logs_section();

    // Last: what led here, which can be long.
    const std::string recent = session_log::recent();
    out += "\n=== last lines logged (oldest first) ===\n" + (recent.empty() ? std::string("(none)\n") : recent);
    return out;
}

std::string save(const std::string& report, std::string* error)
{
    return log_files::save(paths::reports_dir(), log_files::REPORT, report, std::time(nullptr), error);
}

}   // namespace diagnostic
