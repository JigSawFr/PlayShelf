// download_flow — the downloads' queue at work (download/queue): the jobs
// read from queue.json at start-up (one cut short by a restart comes back
// paused, its journal kept), added from the Library tab, installed one
// after the other on a worker thread (install::Installer to the SD card,
// each from the journal of its earlier runs). A library server's
// credentials and pin are taken from its profile when the job starts, and
// go only to that server's addresses.
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

// The setting changed: the console kept awake or not, while a job runs.
void keep_awake_changed();

// Told of every change, on the UI thread; returns an id for unlisten().
int  listen(std::function<void()> changed);
void unlisten(int id);

// "2 downloads · 34 %", "1 paused", "" when the queue is empty.
std::string summary();

}   // namespace download_flow
