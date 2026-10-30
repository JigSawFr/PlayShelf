// PlayShelf — Aurore entry point: loads the preferences, registers the
// custom XML views and shows the tabbed main screen (or the init-error
// screen).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  This program is free software under the GNU
// General Public License v3 or later; it comes with NO WARRANTY. See the
// LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.html> for details.
#include <aurore.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <format>
#include <typeinfo>
#ifndef __SWITCH__
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <aurore/cadran/ui/cut_text.hpp>
#include <aurore/cadran/view/cadran_digits.hpp>
#include <aurore/cadran/view/cadran_icon.hpp>
#include <aurore/cadran/view/loading_bar.hpp>
#include <aurore/cadran/view/scroll_view.hpp>

#include "action/frame_stats_flow.hpp"
#include "action/log_prune_flow.hpp"
#include "action/notices.hpp"
#include "action/rollback_flow.hpp"
#include "activity/init_error_activity.hpp"
#include "activity/loading_activity.hpp"
#include "activity/main_activity.hpp"
#include "activity/screen_hooks.hpp"
#include "app.hpp"
#include "core/platform.h"
#include "core/sim_env.h"
#ifndef __SWITCH__
#include "sim/sim_demo.hpp"
#endif
#include "core/sysinfo.h"
#include "tab/about_tab.hpp"
#include "tab/catalogue_tab.hpp"
#include "tab/downloads_tab.hpp"
#include "tab/home_tab.hpp"
#include "tab/library_tab.hpp"
#include "tab/preferences_tab.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/crash_dump.h"
#include "util/crash_report.hpp"
#include "util/diagnostics.hpp"
#include "util/no_prices.hpp"
#include "util/http.hpp"
#include "util/migrations.hpp"
#include "util/paths.hpp"
#include "util/session_log.hpp"
#include "util/startup.h"
#include "view/cover_placeholder.hpp"
#include "view/loading_dial.hpp"
#include "view/loading_mark.hpp"
#include "view/made_in_france.hpp"
#include "view/notice_bell.hpp"

using namespace aurore::literals;

namespace
{
// The return addresses from the caller up, for a crash file: "" when
// none could be read (desktop).
std::string backtrace_here()
{
    CrashDump d{};
    if (!platform_code_range(&d.module_base, &d.module_size)) d.module_base = d.module_size = 0;
    d.frames = crash_dump_backtrace((uint64_t)(uintptr_t)__builtin_frame_address(0), platform_read64, nullptr, d.frame,
                                    CRASH_DUMP_FRAMES);
    if (d.frames == 0) return "";
    std::string out = "backtrace (innermost first, from the terminate handler):\n";
    for (size_t i = 0; i < d.frames; i++) {
        char where[40];
        crash_dump_where(&d, d.frame[i], where, sizeof(where));
        out += std::format("  #{:<3} {}\n", i, where);
    }
    return out;
}

// An exception nothing caught: what it said, the backtrace and the last
// lines logged go to a file of its own, logs/crashes/crash_<date>.txt
// (util/crash_report.hpp), then PlayShelf ends as a crash, so Atmosphère's
// report shows where it was thrown (the stack is not unwound yet). Left to
// std::terminate, it would abort(), which hbloader takes as a normal exit:
// back in hbmenu, Aurore's task thread, still running, crashed hbmenu
// instead.
[[noreturn]] void on_terminate()
{
    std::string what = "no exception (std::terminate called directly)";
    if (std::exception_ptr e = std::current_exception()) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& x) {
            what = std::string(typeid(x).name()) + ": " + x.what();
        } catch (...) {
            what = "an exception not derived from std::exception";
        }
    }
    const std::string summary = "uncaught " + what;
    const std::string trace = backtrace_here();
    const char* name = crash_report::write(std::time(nullptr), summary.c_str(), trace.empty() ? nullptr : trace.c_str());
    session_log::end_crash(name);
    std::fprintf(stderr, "%s\n", summary.c_str());
    platform_crash();
    std::abort();
}

// "PlayShelf 1.2.0 (abc1234)" for the session and crash files.
std::string build_name()
{
    return std::format("PlayShelf {} ({})", app::version(), app::commit().empty() ? "?" : app::commit());
}

// Every line logged, also in this run's session file (util/session_log.hpp).
void keep_log_lines()
{
    aurore::Logger::getLogEvent()->subscribe([](aurore::Logger::TimePoint when, aurore::LogLevel level, std::string text) {
        static const char LEVELS[] = "EWIDV";
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(when.time_since_epoch()).count() % 1000;
        const unsigned l = (unsigned)level;
        session_log::add(std::chrono::system_clock::to_time_t(when), (int)ms, l < 5 ? LEVELS[l] : '?', text);
    });
}

// This run's session file, and a crash file for the run before it when it
// never ended (no end line: a crash PlayShelf did not catch, a hang closed
// from the HOME menu, the console turned off).
void start_session()
{
    const std::time_t now = std::time(nullptr);
    std::tm tmv{};
    char when[32] = "?";
    if (localtime_r(&now, &tmv)) std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &tmv);
    const std::string header = std::format("=== PlayShelf session ===\nstarted : {}\nbuild   : {}\n\n", when, build_name());
    const std::string name = session_log::start(paths::sessions_dir(), now, header);
    crash_report::prepare(paths::crashes_dir(), build_name(), "", name);
    if (name.empty()) aurore::Logger::warning("logs: no session file in {}", paths::sessions_dir());

    session_log::Unfinished before;
    if (session_log::unfinished(paths::sessions_dir(), name, &before)) {
        const std::string details = "last lines of logs/sessions/" + before.name + ":\n" +
                                    session_log::last_lines(before.text, 60);
        const char* crash = crash_report::write(
            now, ("the run of " + before.name + " did not end: a crash PlayShelf did not catch, closed from the HOME "
                  "menu while it hung, or the console turned off").c_str(),
            details.c_str(), false);
        aurore::Logger::warning("logs: the last run ({}) did not end; {}", before.name,
                              *crash ? std::string("see logs/crashes/") + crash : std::string("no crash file written"));
    }
}

// The console, for the session and crash files: "firmware 22.5.0,
// Atmosphère 1.11.1, sysMMC, over a game".
std::string system_line(const SysInfo& si)
{
    const std::string fw = ui::hos_text(si.hos_version);
    const char* storage = diagnostic::storage_name(sysinfo_storage(&si));
    return std::format("firmware {}, Atmosphère {}, {}, {}", fw,
                       si.ams_valid ? std::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro) : "unknown",
                       storage ? storage : "storage unknown",
                       si.applet_mode ? "from the Album (applet)" : "over a game");
}

// The start-up after the loading screen, one step per frame so the loading
// screen moves along (aurore::sync runs a task after the frame is drawn, and
// one queued from a task waits for the next frame). It took a moment on the
// console (the service probe, the SD-card reads, the main screen's build),
// and the screen stayed black for it. Each step is timed for the diagnostic
// report (util/startup.h).

// The last step: `first` built and shown over the loading screen, which stays
// under it: it holds the focus the first screen hands back, so nothing
// focused is ever freed under the stack (ui::replace_screen pops a lock or
// recovery screen).
void show_first(aurore::Activity* first)
{
    aurore::Application::pushActivity(first, aurore::TransitionAnimation::NONE);
    frame_stats_flow::apply();  // Help & diagnostics › Developer › Frame statistics, when on
    log_prune_flow::run();  // the reports and crashes past the limits of About › Help & diagnostics
    startup_mark(StartupStep_FirstScreen);
    rollback_flow::started();   // this build starts: nothing to go back from
    aurore::Logger::info("start-up: first screen ready after {} ms", startup_elapsed_ms(StartupStep_Main, StartupStep_FirstScreen));
}

// The first screen once the intro is over: checked once a frame.
void after_intro(LoadingActivity* loading, aurore::Activity* first)
{
    if (loading->inIntro()) {
        aurore::sync([loading, first]() { after_intro(loading, first); });
        return;
    }
    startup_mark(StartupStep_Intro);
    show_first(first);
}

// The first screen, once the console is probed. Constructed only:
// show_first builds it.
aurore::Activity* choose_first()
{
    return new MainActivity();
}

void start(LoadingActivity* loading)
{
    startup_mark(StartupStep_LoadingDrawn);
    aurore::Logger::info("start-up: loading screen drawn after {} ms", startup_elapsed_ms(StartupStep_Main, StartupStep_LoadingDrawn));
    if (config::get().intro) {
        loading->startIntro();
        aurore::Logger::info("start-up: intro, {} ms at least", LoadingActivity::INTRO_US / 1000);
    }
    const bool probed = app::init();
    startup_mark(StartupStep_Probed);
    if (!probed) {
        aurore::Logger::error("start-up probe failed (0x{:08X}) — showing InitErrorActivity", app::init_result());
        show_first(new InitErrorActivity());
        return;
    }
    loading->show(LoadingActivity::Step::Settings);
    aurore::sync([loading]() {
        aurore::Activity* first = choose_first();
        startup_mark(StartupStep_Settings);
        loading->show(LoadingActivity::Step::Screen);
#ifndef __SWITCH__
        // Desktop simulation, for a screenshot of the loading screen.
        if (sim_env_flag("PLAYSHELF_SIM_HOLD_LOADING")) {
            aurore::Logger::info("start-up: held on the loading screen (PLAYSHELF_SIM_HOLD_LOADING)");
            delete first;
            return;
        }
#endif
        aurore::sync([loading, first]() { after_intro(loading, first); });
    });
}

// The system in the log and in a crash file.
void log_system()
{
    SysInfo si;
    sysinfo_get(&si);
    const std::string system = system_line(si);
    aurore::Logger::info("start-up: {}, {}", build_name(), system);
    crash_report::prepare(paths::crashes_dir(), build_name(), system, session_log::current_name());
}

// The views the XML layouts name (Cadran's own look, the footer's hints and
// the header's badge and settings come with Aurore).
void register_views()
{
    aurore::Application::registerXMLView("ScrollView",      ScrollView::create);
    aurore::Application::registerXMLView("MadeInFrance",    MadeInFrance::create);
    aurore::Application::registerXMLView("HomeTab",         HomeTab::create);
    aurore::Application::registerXMLView("LibraryTab",      LibraryTab::create);
    aurore::Application::registerXMLView("CatalogueTab",    CatalogueTab::create);
    aurore::Application::registerXMLView("DownloadsTab",    DownloadsTab::create);
    aurore::Application::registerXMLView("PreferencesTab",  PreferencesTab::create);
    aurore::Application::registerXMLView("AboutTab",        AboutTab::create);

    aurore::Application::registerXMLView("LoadingMark",     LoadingMark::create);
    aurore::Application::registerXMLView("LoadingDial",     LoadingDial::create);
    aurore::Application::registerXMLView("LoadingBar",      LoadingBar::create);
    aurore::Application::registerXMLView("CadranIcon",      CadranIcon::create);
    aurore::Application::registerXMLView("CadranDigits",    CadranDigits::create);

    NoticeBell::install();   // the bell goes in the header's row at the right
}

// After the main loop: the tasks and writes still running finished, the
// services closed, the session ended.
void shut_down()
{
    // Network tasks still running (an update check, a sign-in waiting for its
    // code): stopped, and waited for a moment, before what they use goes away.
    // The SD-card writes still queued (ui::write_later) are done first.
    http::abort_all();
    ui::finish_background(std::chrono::seconds(3));
    frame_stats_flow::stop();
    app::shutdown();
    http::cleanup();
    aurore::Logger::info("quit: after {} s", session_log::uptime_s());
    session_log::end("clean exit");
}

#ifndef __SWITCH__
FILE* s_stats = nullptr;   // PLAYSHELF_SIM_STATS's file, when asked (main())
aurore::Time s_stats_last = 0;
#endif

void close_stats()
{
#ifndef __SWITCH__
    if (s_stats) std::fclose(s_stats);
    s_stats = nullptr;
#endif
}

// One frame; false once PlayShelf quits.
bool run_frame()
{
    if (!aurore::Application::mainLoop()) return false;
    // The lines the session file waits for, written off the UI thread
    // once they waited long enough (util/session_log.hpp).
    if (session_log::due()) ui::write_later("session log", []() { session_log::flush(); }, "session_log");
    frame_stats_flow::sample();   // Developer › Frame statistics, when on
    cut_text::sample();           // the desktop's texts cut short (PLAYSHELF_SIM_CUT_TEXT)
#ifndef __SWITCH__
    if (s_stats) {
        int draws = 0, fill = 0, stroke = 0, text = 0;
        nvgDebugGetStats(aurore::Application::getNVGContext(), &draws, &fill, &stroke, &text);
        const aurore::Time now = aurore::getCPUTimeUsec();
        std::fprintf(s_stats, "%lld %lld %d %d %d %d\n", (long long)now, (long long)(now - s_stats_last), draws, fill,
                     stroke, text);
        s_stats_last = now;
    }
#endif
    return true;
}
}   // namespace

int main(int argc, char* argv[])
{
    startup_mark(StartupStep_Main);
    keep_log_lines();
    std::set_terminate(on_terminate);
    app::set_self_path(argc > 0 ? argv[0] : nullptr);
    app::set_open_route(argc, argv);

    // The earlier layouts of the data folder (util/migrations.hpp), the
    // build recorded. Step 1: what the eShop prices left (util/no_prices);
    // it goes with the first release after the one that brings it.
    migrations::set_latest(1);
    migrations::run(paths::migrations_file(),
                    { { 1, "no_prices",
                        []() {
                            return no_prices::clean(paths::config_file(), config::SCHEMA, paths::cache_dir(),
                                                    paths::data_dir() + "/notices_read.txt");
                        } } },
                    app::version());
    startup_mark(StartupStep_Migrations);
#ifndef __SWITCH__
    sim_demo::start();   // the simulators' demo: its answers, and on a first start its server and games
#endif
    start_session();
    startup_mark(StartupStep_Session);
    // A way out of main() that skips the end of this function.
    std::atexit([]() { session_log::end("exit"); });
    // Before anything that could crash: the last start of a build just
    // installed never reached its first screen? Then the previous build is
    // offered (action/rollback_flow.hpp).
    if (rollback_flow::at_start()) {
        session_log::end("exit at start-up (the start-up screen of the previous build)");
        return EXIT_SUCCESS;
    }
    startup_mark(StartupStep_Rollback);
    log_system();
    startup_mark(StartupStep_System);

    // What the kit asks of the app's screens, before any opens.
    install_screen_hooks();

    // Preferences first: their language replaces the console's once Aurore
    // has read it (Aurore's Cadran, in Application::init()).
    config::load();
    startup_mark(StartupStep_Preferences);

    // PlayShelf's own theme keys and image slot, before Aurore starts: the
    // theme file the preferences name, read then, knows them.
    LoadingDial::addColors();
    CoverPlaceholder::declare();

    if (!aurore::Application::init()) {
        aurore::Logger::error("Unable to init Aurore Application");
        session_log::end("exit: Aurore could not start");
        return EXIT_FAILURE;
    }
    startup_mark(StartupStep_Borealis);

    // Aurore starts Cadran itself: the preferences' language (in init()), then
    // their font and theme, the palettes after the design boards, the text
    // size, the footer's hints, the header's version badge and settings, and
    // every screen's look (in createWindow()).
    aurore::Application::createWindow("playshelf/title"_i18n);
    startup_mark(StartupStep_Window);

    // No cross-fade when a screen opens or closes: on the console's LCD the
    // two screens blended for 200 ms read as ghosting. 2 ms, not 0: Aurore
    // still runs (and ends) its animations, in whole milliseconds, and a
    // picker's takes half of it.
    aurore::getStyle().addMetric("aurore/animations/show", 2.0f);

    // We own the quit path (B on the sidebar / error screen).
    aurore::Application::setGlobalQuit(false);

    register_views();
    notices::start();   // what was put away at an earlier start, the kit's news

    // The loading screen first, with nothing slow before it, and drawn at once
    // (no fade: a frame at alpha 0 is a black one). The rest of the start-up
    // runs from the end of that first frame.
    auto* loading = new LoadingActivity();
    aurore::Application::pushActivity(loading, aurore::TransitionAnimation::NONE);
    aurore::sync([loading]() { start(loading); });

#ifndef __SWITCH__
    // Desktop measurement (PLAYSHELF_SIM_STATS=<file>): one line per frame,
    // "<time us> <frame us> <draw calls> <fill tris> <stroke tris> <text
    // tris>", the counts nanovg's own. The FPS limiter is turned off so the
    // frame time is the work alone.
    if (const char* path = sim_env("PLAYSHELF_SIM_STATS")) {
        // Created with an explicit mode (CodeQL: fopen's depends on the umask).
        const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            s_stats = fdopen(fd, "w");
            if (!s_stats) close(fd);
        }
        aurore::Application::setLimitedFPS(0);
    }
    s_stats_last = aurore::getCPUTimeUsec();
#endif
#ifdef __EMSCRIPTEN__
    // In a browser a frame must hand the thread back to the page (web/README.md):
    // the browser calls one per display refresh, and main() returns at once.
    emscripten_set_main_loop([]() {
        if (run_frame()) return;
        emscripten_cancel_main_loop();
        close_stats();
        shut_down();
    }, 0, false);
    return EXIT_SUCCESS;
#else
    while (run_frame()) {}
    close_stats();

    shut_down();
    return EXIT_SUCCESS;
#endif
}
