// Host tests for source/action/write_queue_logic.cpp: the SD-card writes run
// in the order asked, a file written whole keeps only its newest content, a
// file added to keeps every write, a waiter sees its write done (running its
// own file's jobs itself, never another file's, and never two of one file at
// once), config.json can go first, and nothing queued is lost at exit.
#include "check.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "action/write_queue_logic.hpp"

using write_queue_logic::Queue;
using namespace std::chrono_literals;

namespace
{
// What the jobs did, in order (they may run on another thread).
struct Log
{
    std::mutex lock;
    std::string text;
    void add(const std::string& s)
    {
        std::lock_guard<std::mutex> g(lock);
        text += s;
    }
    std::string get()
    {
        std::lock_guard<std::mutex> g(lock);
        return text;
    }
};
}   // namespace

static void test_order()
{
    Queue q;
    Log log;
    bool start = false;
    q.push("", [&]() { log.add("a"); }, &start);
    CHECK(start);   // the first one asks for a runner ...
    q.push("", [&]() { log.add("b"); }, &start);
    CHECK(!start);   // ... the next ones join it
    q.push("", [&]() { log.add("c"); }, &start);
    CHECK(!start);
    CHECK(log.get().empty());   // nothing runs on push
    q.run();
    CHECK(log.get() == "abc");
    // Idle again: the next push asks for a new runner.
    q.push("", [&]() { log.add("d"); }, &start);
    CHECK(start);
    q.run();
    CHECK(log.get() == "abcd");
    q.run();   // a runner with nothing to do
    CHECK(log.get() == "abcd");
}

static void test_latest_wins()
{
    Queue q;
    Log log;
    bool start = false;
    const Queue::Ticket first = q.push("config", [&]() { log.add("1"); }, &start);
    q.push("", [&]() { log.add("h"); }, &start);
    q.push("other", [&]() { log.add("o"); }, &start);
    const Queue::Ticket second = q.push("config", [&]() { log.add("2"); }, &start);
    CHECK(second > first);
    // The older config.json is not written; the newer one after what came between.
    q.run();
    CHECK(log.get() == "ho2");
    // The replaced job's waiter is not left hanging.
    q.wait(first);
    q.wait(second);
    // Appends (no key) are never merged.
    q.push("", [&]() { log.add("x"); }, &start);
    q.push("", [&]() { log.add("x"); }, &start);
    q.run();
    CHECK(log.get() == "ho2xx");
}

static void test_wait_takes_over()
{
    Queue q;
    Log log;
    bool start = false;
    q.push("", [&]() { log.add("a"); }, &start);
    const Queue::Ticket t = q.push("config", [&]() { log.add("b"); }, &start);
    // The runner was asked for but has not started: the waiter runs its own
    // job, and only that one (never another file's, on the UI thread).
    q.wait(t);
    CHECK(log.get() == "b");
    q.run();   // the runner, late: what is left, nothing run twice
    CHECK(log.get() == "ba");
    q.wait(t);   // already run: returns at once
    q.wait(0);
    q.settle();
}

static void test_threads()
{
    Queue q;
    Log log;
    std::atomic<bool> started{ false }, release{ false };
    bool start = false;
    q.push("", [&]() {
        started = true;
        while (!release) std::this_thread::sleep_for(1ms);
        log.add("slow");
    }, &start);
    REQUIRE(start);
    std::thread runner([&q]() { q.run(); });
    while (!started) std::this_thread::sleep_for(1ms);   // the runner is on a long export
    const Queue::Ticket t = q.push("config", [&]() { log.add("cfg+"); }, &start);
    CHECK(!start);
    // config.json is another file: written now by its waiter, not behind the export.
    q.wait(t);
    CHECK(log.get() == "cfg+");
    release = true;
    q.push("", [&]() { log.add("+h"); }, &start);
    q.settle();   // the runner may have gone: settle runs it here then
    CHECK(log.get() == "cfg+slow+h");
    runner.join();
}

static void test_same_file()
{
    // A job of the same file as the one running waits for it: never two
    // writes of one file at once, nor out of order.
    Queue q;
    Log log;
    std::atomic<bool> started{ false }, release{ false };
    bool start = false;
    q.push("config", [&]() {
        started = true;
        while (!release) std::this_thread::sleep_for(1ms);
        log.add("1");
    }, &start);
    std::thread runner([&q]() { q.run(); });
    while (!started) std::this_thread::sleep_for(1ms);
    const Queue::Ticket t = q.push("config", [&]() { log.add("2"); }, &start);
    std::thread waiter([&q, t]() { q.wait(t); });
    std::this_thread::sleep_for(20ms);
    CHECK(log.get().empty());
    release = true;
    waiter.join();
    CHECK(log.get() == "12");
    runner.join();

    // The runner skips a file a waiter is writing, and comes back to it.
    Queue busy;
    Log done;
    std::atomic<bool> in{ false }, out{ false };
    const Queue::Ticket first = busy.push("history", [&]() {
        in = true;
        while (!out) std::this_thread::sleep_for(1ms);
        done.add("h1");
    }, &start, Queue::Write::Append);
    busy.push("history", [&]() { done.add("h2"); }, &start, Queue::Write::Append);
    busy.push("", [&]() { done.add("e"); }, &start);
    std::thread w([&busy, first]() { busy.wait(first); });
    while (!in) std::this_thread::sleep_for(1ms);
    std::thread r([&busy]() { busy.run(); });
    // The export goes past the history while h1 is being written.
    while (done.get().empty()) std::this_thread::sleep_for(1ms);
    CHECK(done.get() == "e");
    out = true;
    w.join();
    r.join();
    CHECK(done.get() == "eh1h2");
}

static void test_settle_key()
{
    Queue q;
    Log log;
    bool start = false;
    q.push("", [&]() { log.add("x"); }, &start);
    q.push("history", [&]() { log.add("1"); }, &start, Queue::Write::Append);
    q.push("pt_log", [&]() { log.add("p"); }, &start, Queue::Write::Append);
    q.push("history", [&]() { log.add("2"); }, &start, Queue::Write::Append);
    // Appends of one file are all kept, in order; only that file's run here.
    q.settle("history");
    CHECK(log.get() == "12");
    q.settle("history");   // nothing left of it: at once
    q.settle("none");
    q.settle();   // every file, here
    CHECK(log.get() == "12xp");
    q.run();
    CHECK(log.get() == "12xp");
}

static void test_first()
{
    // config.json ahead of the jobs still waiting.
    Queue q;
    Log log;
    bool start = false;
    q.push("", [&]() { log.add("e"); }, &start);
    q.push("history", [&]() { log.add("h"); }, &start, Queue::Write::Append);
    q.push("config", [&]() { log.add("1"); }, &start, Queue::Write::Whole, true);
    const Queue::Ticket t = q.push("config", [&]() { log.add("2"); }, &start, Queue::Write::Whole, true);
    q.run();
    CHECK(log.get() == "2eh");   // the newer config.json only, first
    q.wait(t);
}

static void test_flush()
{
    // Queued, the runner never started (no thread to be had at exit): run by flush.
    Queue q;
    Log log;
    bool start = false;
    q.push("", [&]() { log.add("h"); }, &start);
    q.push("config", [&]() { log.add("c"); }, &start);
    CHECK(q.flush(100ms));
    CHECK(log.get() == "hc");
    // After the flush, a write runs at once, on the caller's thread.
    const Queue::Ticket t = q.push("", [&]() { log.add("late"); }, &start);
    CHECK(!start);
    CHECK(t == 0);
    CHECK(log.get() == "hclate");
    q.run();   // a runner asked for before the flush: nothing left to do
    CHECK(log.get() == "hclate");

    // A busy runner: flush waits for it, and every job queued behind it runs.
    Queue busy;
    Log done;
    std::atomic<bool> started{ false }, release{ false };
    busy.push("", [&]() {
        started = true;
        while (!release) std::this_thread::sleep_for(1ms);
        done.add("a");
    }, &start);
    std::thread runner([&busy]() { busy.run(); });
    while (!started) std::this_thread::sleep_for(1ms);
    busy.push("", [&]() { done.add("b"); }, &start);
    CHECK(!busy.flush(20ms));   // still busy after `max`
    // Closed with the runner busy: queued for it, not run beside it.
    busy.push("", [&]() { done.add("c"); }, &start);
    CHECK(!start);
    release = true;
    CHECK(busy.flush(5000ms));
    CHECK(done.get() == "abc");
    runner.join();
}

static void test_throwing_job()
{
    Queue q;
    Log log;
    bool start = false;
    q.push("", []() { throw std::runtime_error("SD card gone"); }, &start);
    const Queue::Ticket t = q.push("", [&]() { log.add("next"); }, &start);
    q.run();
    CHECK(log.get() == "next");
    q.wait(t);
}

int main()
{
    test_order();
    test_latest_wins();
    test_wait_takes_over();
    test_threads();
    test_same_file();
    test_settle_key();
    test_first();
    test_flush();
    test_throwing_job();
    return CHECK_DONE("write_queue_logic: all tests passed");
}
