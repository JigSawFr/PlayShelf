// worker_threads — the threads of the UI's background tasks
// (ui::in_background), each joined once its task is over.
//
// On the console a thread cannot be detached: devkitPro's newlib answers
// ENOSYS to pthread_detach (libnx has no __syscall_thread_detach), so
// std::thread::detach() throws, and the std::thread it leaves joinable ends
// the app with std::terminate as it goes out of scope, before any catch. So
// no thread is detached here: each is kept until its task is over, then
// joined by the next start() or by wait_all(). No borealis: host-tested
// (tests/worker_threads).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class WorkerThreads
{
public:
    using Task    = std::function<void()>;
    using OnThrow = std::function<void(const std::string& what)>;

    // `on_throw` is told of a task that threw (on that task's thread): the
    // task is over all the same.
    explicit WorkerThreads(OnThrow on_throw = {});
    WorkerThreads(const WorkerThreads&)            = delete;
    WorkerThreads& operator=(const WorkerThreads&) = delete;
    // Waits for every task still running, then joins its thread.
    ~WorkerThreads();

    // Runs `task` on a thread of its own, after joining the threads whose task
    // is over. False when no thread could be had (*error says why, in
    // English): `task` did not run, the caller runs it elsewhere.
    bool start(Task task, std::string* error = nullptr);
    // Tasks not over yet.
    int running();
    // Waits up to `max` for every task to be over, then joins the threads of
    // those that are. False when some still run: their threads stay here,
    // unjoined, so this object must outlive them.
    bool wait_all(std::chrono::milliseconds max);

private:
    struct Worker
    {
        std::thread           thread;
        std::shared_ptr<bool> done;   // its task is over; under lock_
    };
    int               running_locked() const;
    // The workers whose task is over, taken out of the list.
    std::list<Worker> take_over_locked();
    static void       join(std::list<Worker>& workers);

    std::mutex              lock_;
    std::condition_variable changed_;
    std::list<Worker>       workers_;
    const OnThrow           on_throw_;
};
