// ui — PlayShelf's own helpers (background tasks, the SD card's writes,
// config.json, result codes, the console's clock and system), beside Aurore's
// Cadran helpers (Aurore/cadran/ui/ui.hpp: dialogs, toasts, pickers…) in
// the same namespace.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

#include <aurore/cadran/ui/ui.hpp>

#include "core/calendar.h"
#include "core/nx_types.h"
#include "core/sysinfo.h"
#include "util/patches.hpp"

namespace ui
{

// "error 0x00001234" followed by a human explanation when one is known.
// PlayShelf's own results (read-only, invalid value …) are just the sentence:
// there is no code to look up. A system code gets, in a paragraph of its own,
// what to do if it happens again (send a report); `next_step` false leaves it
// out (the start-up error and recovery screens say what to do themselves).
std::string rc_text(Result rc, bool next_step = true);

// Runs `task` on a thread of its own, rather than aurore::async: that single
// queue runs one task after the other, so a network request queued there
// waits behind loading game icons (minutes, on a console with many games),
// or behind another request's timeout. Every network task goes here; the
// icons stay on the queue, one at a time on the console's services. Queued
// there when no thread can be had. `what` names it in the log. The thread is never detached (the console
// cannot: util/worker_threads.hpp); it is joined once the task is over.
void in_background(const char* what, std::function<void()> task);
// At exit, after the main loop: says the app is quitting (quitting() turns
// true, for a task that waits or polls) and waits up to `max` for the tasks
// of in_background still running, so none of them outlives the objects it
// uses (aurore::sync's queue, curl, statics), joining the threads of those
// that ended. The writes of write_later are finished first, and any asked
// for later run at once. False when some are still running.
bool finish_background(std::chrono::milliseconds max);
bool quitting();

// One screen of its own at start-up, the first to ask: the update screen
// (once the check answers), "What's new" or the support reminder (a few
// seconds in). True for that one; the others wait for the next start, as when
// something else is in front of the main screen. `what` names it in the log.
bool claim_start_screen(const char* what);

// An SD-card write off the UI thread (a slow card must not hold a frame), on
// one queue run by a task of in_background: the writes happen one at a time,
// in the order asked. `key` names a file written whole: a write of it still
// waiting is dropped for the newer one. `job` reports its own result (ui::
// notify, ui::error). finish_background runs what is still queued at exit.
void write_later(const char* what, std::function<void()> job, const std::string& key = "");
// The same for a file added to (a log): `key`
// names it, and none of its writes is dropped.
void append_later(const char* what, const std::string& key, std::function<void()> job);
// Until every write asked so far is on the SD card, the waiting ones run on
// this thread: off the UI thread only (a report collecting every file).
void settle_writes();
// Until every write of the file `key` asked so far is on the SD card: before
// reading it back (history.json). Its writes still waiting run here; those
// of other files are left to the queue, so a long export does not hold the
// UI thread.
void settle_writes(const std::string& key);

void refuse(Result rc);   // its rc_text
// `what` failed with `rc`: `what` as the dialog's title, the error's hint as
// its text, its code small, and Send a report for a console code ("" for
// `what`: the hint alone). `before`: what else happened, said first (what
// went through, or another failure).
void error(const std::string& what, Result rc, const std::string& before = "");
// Saves the settings (config::snapshot, then config::write off the UI
// thread: write_later): `saved_text` as a toast once it is on the SD card,
// `failed_text` (default: the config_err toast) when the card refused it.
// False only when it cannot be saved at all (an unreadable config.json kept:
// config::snapshot refuses to overwrite it), said at once.
bool save_config(const std::string& saved_text = "", const std::string& failed_text = "");
// The same, waiting for the write: for a record that must be on the SD card
// before the console change it describes. False when it is not. Only for
// config.json: it goes ahead of the other writes still waiting, and runs here
// when the queue is on another file's.
bool save_config_now();
// Success: `ok_text` as a toast. Failure: `error_prefix — reason` as a dialog.
void notify_result(Result rc, const std::string& ok_text, const std::string& error_prefix);

// "Now" on the console: its user clock read live and its time-zone rule
// (calendar.h says why not time()/localtime()).
LocalTime   local_now();
int         today_weekday();                    // 0 = Sunday
std::string today_date();                       // "2026-10-07"
std::string now_hms();                          // "14:03:12"
std::string now_stamp();                        // "2026-10-07 14:03"
// While one lives, local_now() and the four above read the console's clock
// once on the thread that made it (two IPCs each time otherwise): a screen's
// refresh asks the time of day a dozen times, through the helpers it calls.
// Other threads read it live. Nested ones keep the outer one's time.
class SameTime
{
public:
    SameTime();
    ~SameTime();
    SameTime(const SameTime&) = delete;
    SameTime& operator=(const SameTime&) = delete;

private:
    bool outer_;
};

// System strings shared by the About section and the diagnostic report.
std::string hos_text(u32 version);                        // "23.0.1" (MAKEHOSVERSION), "?" for 0
std::string fw_text(const SysInfo& info);                 // "23.0.1 · Atmosphère 1.12.0"

std::string storage_text(const SysInfo& info);             // "emuMMC (Atmosphère)"
std::string storage_short(const SysInfo& info);            // "emuMMC" / "sysMMC" / "—"
// The game-patch report of this boot, read from the SD card once per run (it
// only changes with a reboot).
const patches::Report& patch_report();
// After the patch files were moved: read again.
void                   reread_patch_report();
std::string time_text(uint64_t posix);                     // local time, or "—" for 0

// Read-only mode: a toast says so and this returns true (the caller then
// does nothing). Else false.
bool refuse_read_only();
// A BooleanCell flips before telling its listener: in read-only mode the
// switch would flip, the write be refused and the switch flip back. This
// checks first, then flips as BooleanCell does.
void guard_switch(aurore::BooleanCell* cell);

// After app::set_read_only / set_dev_mode: updates the main screen title,
// tells the app (ui::hooks::mode_changed) and re-reads the tab on screen.
void on_mode_changed();

}   // namespace ui
