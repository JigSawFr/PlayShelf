// ui — shared helpers for dialogs, toasts, keyboards and formatting.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "ui/format_text.hpp"
#include "ui/cadran_text.hpp"
#include "ui/guarded.hpp"
#include "core/calendar.h"
#include "core/nx_types.h"
#include "core/sysinfo.h"
#include "util/patches.hpp"

namespace ui
{

// The status colours below are the boards' inks (cadran::install_theme(),
// which must run before the first lookup: borealis aborts on an unknown
// theme key), one shade per theme (>= 4.5:1).
// On the console: the default font of the labels made from now on, for the
// strings' language `locale`: the system's Latin / Japanese font, the Chinese
// one only as a fallback; for Chinese, the Chinese one. Right after
// createWindow(), before the first label exists, then again when the language
// changes while PlayShelf runs (live_prefs, which builds the screens again).
void     use_font_for(const std::string& locale);

// "error 0x00001234" followed by a human explanation when one is known.
// PlayShelf's own results (read-only, invalid value …) are just the sentence:
// there is no code to look up. A system code gets, in a paragraph of its own,
// what to do if it happens again (send a report); `next_step` false leaves it
// out (the start-up error and recovery screens say what to do themselves).
std::string rc_text(Result rc, bool next_step = true);

// Runs `task` on a thread of its own, rather than brls::async: that single
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
// uses (brls::sync's queue, curl, statics), joining the threads of those
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

// Deferred to the next frame (toasts fired right after a system applet returns
// were dropped on fw 22.1.0).
// What a toast says: news (plain), something done (a check), or something
// not done because of how things are (a warning); the icon in the Cadran look.
enum class Toast
{
    Info,
    Done,
    Refused,
};
void notify(const std::string& text, Toast kind = Toast::Info);
// Something asked that was not done, and why, as a Refused toast (its
// warning icon): a refusal is said and gone, a failure is a dialog (error).
void refuse(const std::string& text);
void refuse(Result rc);   // its rc_text
// A failure the user asked for something and did not get: a dialog, not a
// toast (a toast is gone before "error 0x…" is read). Next frame, so it can
// be called from a dialog button. Logged as "error: " for the smoke test.
void error(const std::string& text);
// `what` failed with `rc`: `what` as the dialog's title, the error's hint as
// its text, its code small, and Send a report for a console code ("" for
// `what`: the hint alone). `before`: what else happened, said first (what
// went through, or another failure).
void error(const std::string& what, Result rc, const std::string& before = "");
// `what` failed because of `why` (a path, a message from the network…).
void error(const std::string& what, const std::string& why);
// The dialog the three open, now: the title, the reason, the code small, and
// Send a report (OK focused) when `f.report`.
void show_failure(const cadran_text::Failure& f);
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
// `next` in place of the current screen. Borealis never pops the first
// activity (the loading screen, under every start screen): a screen that
// cannot be popped gets `next` pushed over it instead.
void replace_screen(brls::Activity* next);
// Success: `ok_text` as a toast. Failure: `error_prefix — reason` as a dialog.
void notify_result(Result rc, const std::string& ok_text, const std::string& error_prefix);

// Two-button dialog. Cancel (left) has the focus when it opens;
// `confirm_label` (right) runs `on_yes`, Cancel or B runs `on_no`.
// `danger`: the confirm button is drawn in the "bad" colour (see below).
void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no = nullptr, bool danger = false);
// Same, for actions that delete something: the confirm button is drawn in the
// "bad" colour so it never looks like an ordinary OK.
void confirm_danger(const std::string& body, const std::string& confirm_label, std::function<void()> on_yes);
void info(const std::string& body);
// A web page to open on a phone: `body`, the QR code of `url`, then `url`
// itself, shortened (text::short_url), and OK.
void qr_dialog(const std::string& body, const std::string& url);

// A list of checks in a dialog: each line with its mark (a teal check for
// what is ready, an amber or coral triangle for what is not, a grey dot for
// what is only said), under `title`, `note` small at the end (may be empty).
enum class Check
{
    Ok,
    Warn,
    Bad,
    Info,
    Header,   // not a check: the name of the panel of lines after it
};
struct CheckLine
{
    Check       state = Check::Info;
    std::string text;
};
// One of them: the mark, then the text (`size` px).
brls::Box* check_row(const CheckLine& line, float size = 17);
// Under `title` (and `subtitle`), the lines in rounded panels, one per
// Header line (its name small above it); `note` with an info icon last.
brls::Box* checklist_body(const std::string& title, const std::vector<CheckLine>& lines, const std::string& note = "",
                          const std::string& subtitle = "");
// A dialog that only says something: a badge (`icon` in `tone`'s colour),
// `title`, `text`, OK.
void notice(const char* icon, Check tone, const std::string& title, const std::string& text);
// `body` with Cancel and `hold_label`: A (or a finger) held on it for 2 s,
// the button filling meanwhile, runs `on_yes`; let go sooner, it empties
// and nothing happens. For what is hard to undo from the console.
void hold_confirm(brls::Box* body, const std::string& hold_label, std::function<void()> on_yes,
                  std::function<void()> on_no = nullptr);

// A dialog showing `text`, laid out by cadran::dialog_body (its question as
// the title, its tables, its notes), the caller adding its buttons; a text
// taller than the dialog's room scrolls.
brls::Dialog* dialog(const std::string& text);
// The same with `below` under the text, the card's width unless it aligns
// itself: a QR code, a code to read out, the cards to support the app.
brls::Dialog* dialog(const std::string& text, brls::View* below);
// The same for a body already laid out (a checklist, a sheet): as it is, or
// in a scrolling frame when taller than the dialog's room.
brls::Dialog* box_dialog(brls::Box* body);

// B closes `dialog` and runs `on_cancel` (borealis' own B only closes it).
void on_cancel(brls::Dialog* dialog, std::function<void()> on_cancel);
// A Cancel button and B, both closing `dialog` and running `cancel`: a
// dialog that waits for something (a check, an answer from the network).
void add_cancel(brls::Dialog* dialog, std::function<void()> cancel);

// A list of values, as a sheet (ui/action_sheet.cpp); `on_pick(index)` runs
// after it has closed. The row
// `selected` has the focus and is checked as the current value, unless
// `checked` is false (a list of things to do, not of values). `details`, when
// given, says more about each row, after its label and quieter; `icons`, what
// goes before each row's label: a Cadran icon's name ("moon"), or a picture
// (a resource, "img/…png", drawn as it is); empty: none.
void pick(const std::string& title, const std::vector<std::string>& values, int selected,
          std::function<void(int)> on_pick, bool checked = true, const std::vector<std::string>& details = {},
          const std::vector<std::string>& icons = {});
// The same as tiles, `columns` to a row, each its Cadran icon (`icons`, a
// name) large above its label: a few values that read better side by side
// (the tabs of Start on).
void pick_grid(const std::string& title, const std::vector<std::string>& values, const std::vector<std::string>& icons,
               int selected, std::function<void(int)> on_pick, int columns = 4);
// The same with several rows checked: those `marks` sets (true). Choosing a
// row closes the list as pick() does: to toggle one, the caller opens it
// again with the row's mark changed and the focus on it (`focus`).
void pick_marked(const std::string& title, const std::vector<std::string>& values, const std::vector<bool>& marks,
                 int focus, std::function<void(int)> on_pick, const std::vector<std::string>& details = {});
// A list of things to do, as a sheet (ui/action_sheet.cpp): each line's
// label, and what it runs once the sheet has closed; its icon (a Cadran
// icon, nullptr: none), what is set now at its end (`detail`), its group
// (a section starts where it changes, its name the kicker above it) and its
// tone (ON: the detail in teal, WARN: amber, DANGER: a removal, in coral);
// switched(): a switch at its end, on or off, instead of the detail. A line
// without `run` only closes the sheet. `subtitle`, under the title: what it
// is about now; `image`, an NVG image before the title (someone else's,
// kept). The first line is selected; none, no sheet.
struct Choice
{
    enum class Tone { PLAIN, ON, WARN, DANGER };
    Choice(std::string text, std::function<void()> action, const char* icon_name = nullptr, std::string detail_text = "",
           std::string group_name = "", Tone line_tone = Tone::PLAIN)
        : label(std::move(text)), run(std::move(action)), icon(icon_name), detail(std::move(detail_text)),
          group(std::move(group_name)), tone(line_tone)
    {
    }
    Choice& switched(bool on)
    {
        this->toggle = on ? 1 : 0;
        return *this;
    }
    std::string           label;
    std::function<void()> run;
    const char*           icon;
    std::string           detail;
    std::string           group;
    Tone                  tone;
    int                   toggle = -1;   // switched(): 1 on, 0 off
};
void pick_actions(const std::string& title, const std::vector<Choice>& choices, const std::string& subtitle = "",
                  int image = 0);
// The sheet those open: its view, on top of the activities' stack.
bool is_sheet(brls::View* view);

// The system keyboard for a text. The callback only on confirm.
void prompt_text(const std::string& header, const std::string& initial, int max_len,
                 std::function<void(std::string)> on_value);

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
// An action that writes stays visible in read-only mode, greyed (title and
// value), so the parent sees what exists; its handler calls
// refuse_read_only(). `title` / `detail`: the colours when writable. A
// BooleanCell gets its own colours back from its state.
void show_writable(brls::DetailCell* cell, bool writable, NVGcolor title, NVGcolor detail);
void show_writable(brls::DetailCell* cell, bool writable);   // plain text, neutral value
// A BooleanCell flips before telling its listener: in read-only mode the
// switch would flip, the write be refused and the switch flip back. This
// checks first, then flips as BooleanCell does.
void guard_switch(brls::BooleanCell* cell);

// Hides a view when `visible` is false (Visibility::GONE frees its space).
// When the hidden view had the focus, the focus moves to the nearest visible
// focusable sibling (the next one on a tie), so it never lands on the list
// container or jumps far away.
void set_visible(brls::View* view, bool visible);

// Applies several visibility changes, showing views before hiding others, so a
// cell that replaces the focused one (Unlock -> Lock now) is there to take
// the focus.
void set_visible_all(std::initializer_list<std::pair<brls::View*, bool>> changes);

// A label's text, or a cell's detail, set only when it changes: borealis lays
// the whole screen out again on every setText, and the tabs that refresh
// themselves every few seconds mostly write the same values back.
void set_text(brls::Label* label, const std::string& text);
void set_detail(brls::DetailCell* cell, const std::string& text);

// Runs `fill`, which adds many views to `box` (or changes them), with the box
// out of the screen's layout, then lays the screen out once. borealis lays
// the whole screen out again on each addView and setText: a list of n rows
// built in place cost n layouts of the whole screen, n of them on screen.
void batch_layout(brls::Box* box, const std::function<void()>& fill);

// For an activity's onResume(): borealis then gives the focus back to the
// view that opened the dialog (or screen) just closed, and gives none when
// that view was hidden meanwhile; the focus would stay on the closed
// dialog's button, deleted after its animation, and no button answer any
// more. In that case only, `fallback` (else the activity's first focusable
// view) gets it first.
void keep_focus(brls::Activity* activity, brls::View* fallback = nullptr);

// Whether `activity` has the focus: nothing (a dialog, another screen) is
// open over it. Unlike the top of borealis' stack, true again as soon as a
// dialog over it starts closing.
bool has_focus(brls::Activity* activity);
// Whether `activity` is the top of borealis' stack: no screen or dialog
// pushed over it, one closing included.
bool on_top(brls::Activity* activity);
// The main screen, wherever it is in the stack; nullptr before it opens.
brls::Activity* main_screen();
// Shows its tab `position` (ui::tab) and focuses its content: the view with
// id `row` when given and shown, else its first one. Nothing before it opens.
void open_tab(int position, const std::string& row = "");
// Re-reads the tab on screen, if any (after the read-only or developer mode
// changed, the language or the text size).
void refresh_shown_tab();

// Opens another tab of the main screen and focuses its first item. `from` is
// any view inside the current tab; the switch happens on the next frame
// because the current tab is deleted by it.
void go_to_tab(brls::View* from, int position);
// `content` (a tab's view, such as the network clock) on a screen of its own,
// over the current one, titled `title`; B closes it.
void open_screen(brls::View* content, const std::string& title);

// The language and theme pickers, shared by Preferences and the main
// screen's header: save the choice and apply it at once (a language or a
// theme makes every screen again, live_prefs; the text size runs `after`, a
// refresh). language_name / theme_name: the choice saved, as text;
// text_size_name, language_name and theme_name their shorter words on a tile
// (Preferences).
void        pick_language(std::function<void()> after = nullptr);
void        pick_theme(std::function<void()> after = nullptr);
void        pick_text_size(std::function<void()> after = nullptr);
std::string text_size_name(bool tile = false);
std::string language_name(bool tile = false);
std::string theme_name(bool tile = false);
// − on the main screen: a list of the two, each opening its picker.
void        quick_settings(std::function<void()> after = nullptr);

// After app::set_read_only / set_dev_mode: updates the main screen title,
// tells the app (ui::hooks::mode_changed) and re-reads the tab on screen.
void on_mode_changed();

}   // namespace ui
