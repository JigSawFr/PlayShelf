// ui — result texts, background tasks, the SD card's writes, config.json and
// the main screen's state (Aurore's Cadran has the toasts and dialogs). The other ui/*.cpp hold the rest of ui.hpp by concern.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include <aurore/cadran/ui/cadran_modal.hpp>
#include <aurore/cadran/ui/cadran_text.hpp>
#include <aurore/cadran/ui/effects.hpp>
#include <aurore/cadran/ui/hooks.hpp>

#include "action/error_hint_logic.hpp"
#include "action/write_queue_logic.hpp"
#include "ui/app_hooks.hpp"
#include "util/config.hpp"
#include "util/worker_threads.hpp"

#include <atomic>
#include <chrono>
#include <fmt/format.h>

using namespace aurore::literals;

namespace ui
{

static std::string hint_for(Result rc)
{
    const std::string key = error_hint_logic::hint_key(rc);
    return key.empty() ? std::string() : aurore::getStr("playshelf/error/" + key);
}

std::string rc_text(Result rc, bool next_step)
{
    std::string hint = hint_for(rc);
    if (NXM_IS_APP_RESULT(rc) && !hint.empty()) {
        if (hint[0] >= 'a' && hint[0] <= 'z') hint[0] = (char)(hint[0] - 'a' + 'A');
        return hint;
    }
    std::string code = fmt::format("0x{:08X}", (unsigned)rc);
    std::string text = hint.empty() ? aurore::getStr("playshelf/error/code", code)
                                    : aurore::getStr("playshelf/error/code_hint", code, hint);
    // A system code alone leaves the parent stuck: say what to do next. App
    // results already explain themselves.
    if (next_step && !NXM_IS_APP_RESULT(rc)) text += "\n\n" + "playshelf/error/next_step"_i18n;
    return text;
}

namespace
{
std::atomic<bool> s_quitting{ false };
write_queue_logic::Queue s_writes;
std::atomic<bool> s_config_saved{ true };   // the last config.json write went through

// The threads of in_background, each joined once its task is over
// (util/worker_threads.hpp: the console cannot detach a thread). Never
// destroyed: a task still running when the app exits (finish_background gave
// up waiting for it) would be waited for by the destructor, at exit, with no
// screen to say why.
WorkerThreads& workers()
{
    static WorkerThreads* const instance = []() {
        auto* threads = new WorkerThreads(
            [](const std::string& what) { aurore::Logger::error("background task threw: {}", what); });
        // Work Aurore cannot see: while a task runs, the screen has not
        // settled (Application::isSettled(), which the smoke test waits on
        // through AURORE_SETTLE_FILE), its result still to show.
        aurore::Application::setBusyCheck([threads]() { return threads->running() > 0; });
        return threads;
    }();
    return *instance;
}
}   // namespace

void in_background(const char* what, std::function<void()> task)
{
    std::string why;
    if (workers().start(task, &why)) return;
    aurore::Logger::warning("{}: no thread ({}), queued instead", what, why);
    aurore::async(task);
}

bool quitting()
{
    return s_quitting;
}

bool claim_start_screen(const char* what)
{
    static const char* s_claimed = nullptr;
    if (s_claimed) {
        aurore::Logger::info("start-up: {} waits for the next start ({} was shown)", what, s_claimed);
        return false;
    }
    s_claimed = what;
    return true;
}

bool finish_background(std::chrono::milliseconds max)
{
    s_quitting = true;
    const auto until = std::chrono::steady_clock::now() + max;
    // The SD-card writes first: what the parent changed last is not lost.
    if (!s_writes.flush(max)) aurore::Logger::warning("quitting with SD-card writes still running");
    // The rest of `max`, the writes' share taken off.
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - std::chrono::steady_clock::now());
    const bool done = workers().wait_all(left > std::chrono::milliseconds(0) ? left : std::chrono::milliseconds(0));
    if (!done) aurore::Logger::warning("quitting with {} background task(s) still running", workers().running());
    return done;
}

using Write = write_queue_logic::Queue::Write;

static uint64_t push_write(const char* what, std::function<void()> job, const std::string& key,
                           Write write = Write::Whole, bool first = false)
{
    bool start = false;
    const uint64_t ticket = s_writes.push(key, std::move(job), &start, write, first);
    if (start) in_background(what, []() { s_writes.run(); });
    return ticket;
}

void write_later(const char* what, std::function<void()> job, const std::string& key)
{
    push_write(what, std::move(job), key);
}

void append_later(const char* what, const std::string& key, std::function<void()> job)
{
    push_write(what, std::move(job), key, Write::Append);
}

void settle_writes()
{
    s_writes.settle();
}

void settle_writes(const std::string& key)
{
    s_writes.settle(key);
}

// A refusal: the line that asked shakes its head (its focus ring, as the
// console does at the end of a list), with the toast that says why.
static void shake_focus()
{
    if (!effects::motion()) return;
    if (aurore::View* focus = aurore::Application::getCurrentFocus()) focus->shakeHighlight(aurore::FocusDirection::LEFT);
}

// The texts of a result are read on the UI thread, in the sync: a language
// loaded while the app runs replaces the translations there, which a worker
// thread must not read meanwhile.

void refuse(Result rc)
{
    aurore::sync([rc]() {
        const std::string text = rc_text(rc);
        aurore::Logger::info("toast: {}", text);
        shake_focus();
        aurore::Application::notify(text, cadran::toast_icon(false));
    });
}

static void show_error(const std::string& text)
{
    aurore::Logger::info("error: {}", text);
    cadran_text::Failure f;
    f.reason = text;
    show_failure(f);
}

void error(const std::string& what, Result rc, const std::string& before)
{
    aurore::sync([what, rc, before]() {
        // Logged as the text the dialog says, for the smoke test.
        aurore::Logger::info("error: {}{}", before.empty() ? "" : before + " / ",
                           what.empty() ? rc_text(rc) : what + " \u2014 " + rc_text(rc));
        cadran_text::Failure f = cadran_text::failure(what, hint_for(rc), (uint32_t)rc, NXM_IS_APP_RESULT(rc));
        if (!before.empty()) f.reason = f.reason.empty() ? before : before + "\n\n" + f.reason;
        show_failure(f);
    });
}

// config.json and state.json as they are now, queued (*ticket to wait for). False when it
// cannot be saved at all.
static bool queue_config(const std::string& saved_text, const std::string& failed_text, uint64_t* ticket)
{
    const std::string failed = failed_text.empty() ? "playshelf/toast/config_err"_i18n : failed_text;
    config::Snapshot text;
    if (!config::snapshot(text)) {
        notify(failed, Toast::Refused);
        return false;
    }
    // Ahead of the writes still waiting (an export, the history): what the
    // parent just set is what a crash must not lose, and save_config_now()
    // waits for it.
    *ticket = push_write("config", [text, saved_text, failed]() {
        const bool saved = config::write(text);
        s_config_saved = saved;
        aurore::sync([saved]() {
            if (app_hooks::get().note_write) app_hooks::get().note_write("config", saved);
        });
        if (!saved) notify(failed, Toast::Refused);
        else if (!saved_text.empty()) notify(saved_text, Toast::Done);
    }, "config", Write::Whole, true);
    return true;
}

bool save_config(const std::string& saved_text, const std::string& failed_text)
{
    uint64_t ticket;
    return queue_config(saved_text, failed_text, &ticket);
}

bool save_config_now()
{
    uint64_t ticket;
    if (!queue_config("", "", &ticket)) return false;
    s_writes.wait(ticket);
    return s_config_saved;
}

void notify_result(Result rc, const std::string& ok_text, const std::string& error_prefix)
{
    if (R_SUCCEEDED(rc)) notify(ok_text, Toast::Done);
    else aurore::sync([rc, error_prefix]() { show_error(error_prefix + " — " + rc_text(rc)); });
}

// "PlayShelf" and its icons, on the main screen.
static void update_title()
{
    aurore::Activity* main = main_screen();
    if (main && hooks::get().update_title) hooks::get().update_title(main);
}

void on_mode_changed()
{
    // The frame statistics only run in developer mode.
    if (app_hooks::get().mode_changed) app_hooks::get().mode_changed();
    update_title();
    refresh_shown_tab();
}

}   // namespace ui
