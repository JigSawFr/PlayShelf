// Host tests for source/util/worker_threads.cpp: each task runs on a thread
// of its own, in parallel with the others; a task still running is waited
// for up to the time given, and no longer; a task may start another; the
// destructor waits for the tasks still running; a task that throws is over
// all the same.
#include "check.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

#include "util/worker_threads.hpp"

using namespace std::chrono_literals;

namespace
{
// Where a task waits until the test lets it go on.
struct Gate
{
    std::mutex lock;
    std::condition_variable opened;
    bool open = false;
    void wait()
    {
        std::unique_lock<std::mutex> l(lock);
        opened.wait(l, [this]() { return open; });
    }
    void release()
    {
        {
            std::lock_guard<std::mutex> l(lock);
            open = true;
        }
        opened.notify_all();
    }
};
}   // namespace

static void test_own_thread()
{
    WorkerThreads w;
    std::atomic<int> runs{ 0 };
    std::mutex lock;
    std::set<std::thread::id> ids;
    // The tasks stay at the gate until all three started: start() joins a
    // finished thread before the next one, and a joined thread's id can be
    // given to the next (glibc reuses it), so three tasks that end one after
    // the other could show fewer than three ids.
    Gate gate;
    for (int i = 0; i < 3; i++) {
        CHECK(w.start([&]() {
            runs++;
            {
                std::lock_guard<std::mutex> l(lock);
                ids.insert(std::this_thread::get_id());
            }
            gate.wait();
        }));
    }
    gate.release();
    CHECK(w.wait_all(5s));
    CHECK(runs == 3);
    CHECK(w.running() == 0);
    CHECK(ids.size() == 3);                          // three threads ...
    CHECK(!ids.count(std::this_thread::get_id()));   // ... none of them this one
}

static void test_running_tasks()
{
    WorkerThreads w;
    Gate gate;
    std::atomic<int> over{ 0 };
    CHECK(w.start([&]() { gate.wait(); over++; }));
    CHECK(w.start([&]() { gate.wait(); over++; }));
    CHECK(w.start([&]() { over++; }));   // over at once
    CHECK(w.running() >= 2);             // the third may be over already
    CHECK(!w.wait_all(50ms));            // two still wait at the gate ...
    CHECK(w.running() == 2);             // ... the third is joined by now
    CHECK(over == 1);
    gate.release();
    CHECK(w.wait_all(5s));
    CHECK(over == 3);
    CHECK(w.running() == 0);
}

static void test_tasks_in_parallel()
{
    // Each waits for the other to have started: on one queue, one after the
    // other, the first would wait in vain.
    WorkerThreads w;
    std::atomic<int> arrived{ 0 };
    const auto meet = [&]() {
        arrived++;
        const auto until = std::chrono::steady_clock::now() + 5s;
        while (arrived < 2 && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
    };
    const auto started = std::chrono::steady_clock::now();
    CHECK(w.start(meet));
    CHECK(w.start(meet));
    CHECK(w.wait_all(5s));
    CHECK(arrived == 2);
    CHECK(std::chrono::steady_clock::now() - started < 4s);
}

static void test_task_starts_task()
{
    // Started from a task, before that task is over: wait_all sees both.
    WorkerThreads w;
    std::atomic<int> runs{ 0 };
    CHECK(w.start([&]() {
        runs++;
        CHECK(w.start([&]() { runs++; }));
    }));
    CHECK(w.wait_all(5s));
    CHECK(runs == 2);
    CHECK(w.running() == 0);
}

static void test_wait_all_no_time()
{
    WorkerThreads w;
    Gate gate;
    CHECK(w.start([&]() { gate.wait(); }));
    CHECK(!w.wait_all(0ms));   // no time given: says it still runs, at once
    CHECK(w.running() == 1);
    gate.release();
    CHECK(w.wait_all(5s));
    CHECK(w.running() == 0);
}

static void test_destructor_waits()
{
    std::atomic<bool> over{ false };
    {
        WorkerThreads w;
        CHECK(w.start([&]() {
            std::this_thread::sleep_for(100ms);
            over = true;
        }));
    }
    CHECK(over);   // the destructor waited for it
}

static void test_task_throws()
{
    // A task that throws is over all the same: said to on_throw, waited for,
    // joined, and the next task runs.
    std::mutex lock;
    std::string said;
    WorkerThreads w([&](const std::string& what) {
        std::lock_guard<std::mutex> l(lock);
        said = what;
    });
    CHECK(w.start([]() { throw std::runtime_error("boom"); }));
    CHECK(w.wait_all(5s));
    CHECK(w.running() == 0);
    {
        std::lock_guard<std::mutex> l(lock);
        CHECK(said == "boom");
    }
    CHECK(w.start([]() { throw 42; }));
    CHECK(w.wait_all(5s));
    {
        std::lock_guard<std::mutex> l(lock);
        CHECK(said == "unknown exception");
    }
    std::atomic<bool> ran{ false };
    CHECK(w.start([&]() { ran = true; }));
    CHECK(w.wait_all(5s));
    CHECK(ran);
    // No on_throw: still caught.
    WorkerThreads quiet;
    CHECK(quiet.start([]() { throw std::runtime_error("unheard"); }));
    CHECK(quiet.wait_all(5s));
    CHECK(quiet.running() == 0);
}

int main()
{
    test_own_thread();
    test_running_tasks();
    test_tasks_in_parallel();
    test_task_starts_task();
    test_wait_all_no_time();
    test_destructor_waits();
    test_task_throws();
    return CHECK_DONE("worker_threads tests passed");
}
