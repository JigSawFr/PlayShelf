// write_queue_logic — the SD-card writes of the UI (config.json, the change
// history, the play-timer log, the exports) in one queue, run off the UI
// thread, in the order they were asked for. It has no thread of its own:
// push() says when one must be started to run() it (ui uses in_background).
// A job names the file it writes (its key): two jobs of one file never run
// together, nor out of order, and a waiter (wait, settle of a key) runs the
// jobs of its own file itself rather than wait behind those of other files.
// Host-tested (tests/write_queue_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace write_queue_logic
{

class Queue
{
public:
    using Job    = std::function<void()>;
    using Ticket = std::uint64_t;   // 0: already run

    // How a job writes the file of its key.
    enum class Write
    {
        Whole,    // written whole (config.json): a job of the same key still
                  // waiting is dropped, as this one writes newer content, and
                  // its ticket now waits for this one
        Append,   // added to (history.json, the play-timer log): never dropped
    };

    // Adds `job` at the back, or at the front with `first` (ahead of the jobs
    // still waiting; the one running is not stopped). `key` names the file it
    // writes; "" for the jobs that name none (exports, removals), which run
    // one after the other too. *start: no runner is on the queue, the caller
    // must call run() on a thread of its own. After flush(), `job` runs here
    // and now.
    Ticket push(const std::string& key, Job job, bool* start, Write write = Write::Whole, bool first = false);
    // The runner: runs the jobs until there are none left. Returns at once
    // when the queue was taken over (flush) before it started.
    void run();
    // Until the job of `ticket` (or the newer one that replaced it) has run.
    // The jobs of its key waiting before it, and itself, run here as soon as
    // no other job of that key is running: a waiter neither waits for a
    // runner that may come late, nor runs the jobs of other files.
    void wait(Ticket ticket);
    // Until every job pushed so far has run, running here what waits when
    // the runner has not started (it runs the jobs of every file: a
    // background thread's, not the UI's).
    void settle();
    // Until every job of `key` pushed so far has run, running them here as
    // wait() does (the jobs of other keys are left to the runner).
    void settle(const std::string& key);
    // At exit: waits up to `max` for the runner, runs here what is still
    // queued, and from then on runs each push() at once. False when the
    // runner was still busy after `max`.
    bool flush(std::chrono::milliseconds max);

private:
    struct Item
    {
        std::string key;
        Job job;
        std::vector<Ticket> tickets;
    };
    enum class Runner { None, Asked, Running };

    void run_locked(std::unique_lock<std::mutex>& lock);
    // The first waiting job whose key no running job has (end(): none), and
    // the same restricted to `key`.
    std::deque<Item>::iterator next_free();
    std::deque<Item>::iterator next_free(const std::string& key);
    // Runs `it` here, the lock released meanwhile.
    void run_item(std::unique_lock<std::mutex>& lock, std::deque<Item>::iterator it);
    // Whether a job of `key` (any key with `all`) pushed up to `last` has not run yet.
    bool waiting(Ticket last, const std::string& key, bool all) const;
    void settle_locked(std::unique_lock<std::mutex>& lock, Ticket last, const std::string& key, bool all);

    std::mutex lock_;
    std::condition_variable changed_;
    std::deque<Item> items_;
    std::map<Ticket, std::string> pending_;   // tickets whose job has not run yet, with its key
    std::multiset<std::string> busy_;         // the keys of the jobs running now
    Ticket last_   = 0;
    Runner runner_ = Runner::None;
    bool closed_   = false;
};

}   // namespace write_queue_logic
