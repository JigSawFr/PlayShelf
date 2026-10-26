// download_flow — the downloads' queue at work (download/queue): the jobs
// read from queue.json at start-up (one cut short by a restart comes back
// paused, its journal kept), added from the Catalogue and Library tabs,
// installed one after the other on a worker thread (install::Installer to
// the SD card, each from the journal of its earlier runs). A library server's
// credentials and pin are taken from its profile when the job starts, and
// go only to that server's addresses.
//
// A failed job (download/recovery): a transfer cut short pauses the install
// rather than undoing it, and PlayShelf tries again from there by itself
// (10 s, 30 s, 2 min later); what needs the user is offered (the server's
// other address, its profile, the package found again in its catalogue). A
// job whose server was removed fails before it starts; one whose package
// the console has by now is done.
//
// Before the first job of a run: whether the console patches signature
// checks (util/patches; asked to go on when nothing is found). Before each
// job: the placeholders no job keeps are deleted. While jobs run, the
// console does not go to sleep by itself (Downloads › Stay awake while
// downloading, core/awake).
//
// Everything here runs on the UI thread; listeners are told of every
// change (a job's state, its progress at most twice a second).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "download/queue.hpp"
#include "download/recovery.hpp"
#include "library/servers.hpp"

namespace download_flow
{

// Reads the queue (at start-up, once the main screen is up).
void init();

const std::vector<download::Job>& jobs();

// The running job's live figures.
struct Live
{
    std::string           id;   // "" when none runs
    uint64_t              done = 0, total = 0;
    double                speed = 0;   // bytes a second
    std::optional<double> eta;         // seconds left
    double                quiet = 0;   // seconds since its last byte
};
Live live();

// Adds these jobs at the end (a package already waiting or running is not
// added twice), saves, and starts the queue. Returns how many were added.
size_t add(std::vector<download::Job> jobs);

void pause(const std::string& id);
void resume(const std::string& id);      // a paused or failed job: waiting again
void accept_unsigned(const std::string& id);   // failed on a content not signed by Nintendo: again, taking it
// Removes it, what its runs installed undone (a running one is cancelled).
void cancel(const std::string& id);
void pause_all();
void resume_all();
void clear_finished();

// A failed job PlayShelf tries again by itself (a cut, a busy server; at
// most download::RETRIES times): the seconds before it does, -1 when not.
double retry_in(const download::Job& job);
// What the user can do about a failed job (download/recovery), besides
// removing it; and those that need the flow:
std::vector<download::Remedy> remedies(const download::Job& job);
std::optional<library::Server> server_of(const download::Job& job);   // the job's server, as servers.json has it
void use_other_address(const std::string& id);   // the server's other address (LAN ↔ WAN), then waiting again
// Its server's catalogue read again, the package found in it (its title at
// that version): its new address taken, then waiting again.
void find_again(const std::string& id);

// The setting changed: the console kept awake or not, while a job runs.
void keep_awake_changed();

// Told of every change, on the UI thread; returns an id for unlisten().
int  listen(std::function<void()> changed);
void unlisten(int id);

// "2 to install · 34 %", "1 paused", "1 failed", "" when nothing is left
// to do.
std::string summary();
// The summary in the header's pill, whichever tab is shown (amber when a
// job failed or waits paused, or the running one stands still); none when
// the queue is empty.
void show_pill();

}   // namespace download_flow
