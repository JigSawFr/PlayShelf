// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/write_queue_logic.hpp"

namespace write_queue_logic
{

Queue::Ticket Queue::push(const std::string& key, Job job, bool* start, Write write, bool first)
{
    *start = false;
    std::unique_lock<std::mutex> lock(lock_);
    if (closed_ && runner_ == Runner::None) {
        // Past the exit flush: no runner will come. Still one job of a file at a time.
        changed_.wait(lock, [this, &key]() { return !busy_.count(key); });
        items_.push_front(Item{ key, std::move(job), {} });
        run_item(lock, items_.begin());
        return 0;
    }
    Item item{ key, std::move(job), { ++last_ } };
    if (!key.empty() && write == Write::Whole) {
        for (auto it = items_.begin(); it != items_.end(); ++it) {
            if (it->key != key) continue;
            item.tickets.insert(item.tickets.end(), it->tickets.begin(), it->tickets.end());
            items_.erase(it);
            break;
        }
    }
    pending_[last_] = key;
    if (first) items_.push_front(std::move(item));
    else items_.push_back(std::move(item));
    if (runner_ == Runner::None && !closed_) {
        runner_ = Runner::Asked;
        *start  = true;
    }
    return last_;
}

std::deque<Queue::Item>::iterator Queue::next_free()
{
    // The jobs of a key wait in their order: the first whose key is free is
    // the next one of its file.
    for (auto it = items_.begin(); it != items_.end(); ++it)
        if (!busy_.count(it->key)) return it;
    return items_.end();
}

std::deque<Queue::Item>::iterator Queue::next_free(const std::string& key)
{
    if (busy_.count(key)) return items_.end();
    for (auto it = items_.begin(); it != items_.end(); ++it)
        if (it->key == key) return it;
    return items_.end();
}

void Queue::run_item(std::unique_lock<std::mutex>& lock, std::deque<Item>::iterator it)
{
    Item item = std::move(*it);
    items_.erase(it);
    busy_.insert(item.key);
    lock.unlock();
    // One job failing must not stop the ones behind it.
    try {
        item.job();
    } catch (...) {
    }
    lock.lock();
    busy_.erase(busy_.find(item.key));
    for (Ticket t : item.tickets) pending_.erase(t);
    changed_.notify_all();
}

void Queue::run_locked(std::unique_lock<std::mutex>& lock)
{
    runner_ = Runner::Running;
    for (;;) {
        const auto it = next_free();
        if (it != items_.end()) {
            run_item(lock, it);
            continue;
        }
        if (items_.empty()) break;
        // What is left is of a file a waiter writes right now.
        changed_.wait(lock);
    }
    runner_ = Runner::None;
    changed_.notify_all();
}

void Queue::run()
{
    std::unique_lock<std::mutex> lock(lock_);
    if (runner_ != Runner::Asked) return;   // taken over by flush()
    run_locked(lock);
}

void Queue::wait(Ticket ticket)
{
    std::unique_lock<std::mutex> lock(lock_);
    const auto found = pending_.find(ticket);
    if (found == pending_.end()) return;
    const std::string key = found->second;
    while (pending_.count(ticket)) {
        // The next job of its file, until its own has run: never another file's.
        const auto it = next_free(key);
        if (it != items_.end()) run_item(lock, it);
        else changed_.wait(lock);
    }
}

bool Queue::waiting(Ticket last, const std::string& key, bool all) const
{
    for (const auto& p : pending_) {
        if (p.first > last) break;
        if (all || p.second == key) return true;
    }
    return false;
}

void Queue::settle_locked(std::unique_lock<std::mutex>& lock, Ticket last, const std::string& key, bool all)
{
    while (waiting(last, key, all)) {
        // Every file: only in place of a runner not started yet, as a
        // removal (Delete the log files) must not run beside a write.
        const auto it = !all ? next_free(key) : runner_ == Runner::Running ? items_.end() : next_free();
        if (it != items_.end()) run_item(lock, it);
        else changed_.wait(lock);
    }
}

void Queue::settle()
{
    std::unique_lock<std::mutex> lock(lock_);
    settle_locked(lock, last_, "", true);
}

void Queue::settle(const std::string& key)
{
    std::unique_lock<std::mutex> lock(lock_);
    settle_locked(lock, last_, key, false);
}

bool Queue::flush(std::chrono::milliseconds max)
{
    std::unique_lock<std::mutex> lock(lock_);
    closed_ = true;
    if (runner_ == Runner::Asked) run_locked(lock);
    return changed_.wait_for(lock, max, [this]() { return runner_ == Runner::None && busy_.empty(); });
}

}   // namespace write_queue_logic
