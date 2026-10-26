// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/split_stream.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace install
{
namespace
{
// A piece taken by a connection: its bytes as they arrive.
struct Piece
{
    std::vector<uint8_t> data;
    size_t               filled = 0;
    bool                 done = false;
};

// What the connections and the caller share, under `lock`.
struct Shared
{
    std::mutex                lock;
    std::condition_variable   moved;
    std::map<uint64_t, Piece> held;     // the pieces taken and not handed on yet
    uint64_t                  next = 0;      // the next piece to take
    uint64_t                  handed = 0;    // the piece being handed on
    std::atomic<bool>         stop{false};
    bool                      failed = false;
    std::string               error;
};

// The size of slices handed to the sink: its progress and its pause checked
// that often.
constexpr size_t SLICE = 256u << 10;
}   // namespace

bool split_stream(uint64_t offset, uint64_t length, int connections, const RangeFetch& fetch,
                  const std::function<bool(const uint8_t*, size_t)>& sink, std::string* why,
                  const std::atomic<bool>* cancel, uint64_t chunk)
{
    if (!length) return true;
    if (!chunk) chunk = SPLIT_CHUNK;
    connections = std::max(1, std::min(connections, MAX_CONNECTIONS));
    const uint64_t pieces = (length + chunk - 1) / chunk;
    if (connections == 1 || pieces < 2) return fetch(offset, length, sink, why, cancel);

    Shared s;
    auto   size_of = [&](uint64_t i) { return (size_t)std::min(chunk, length - i * chunk); };
    auto   worker = [&]() {
        for (;;) {
            uint64_t i = 0;
            Piece*   p = nullptr;
            {
                std::unique_lock<std::mutex> l(s.lock);
                // At most `connections` pieces held, the one handed on included.
                s.moved.wait(l, [&]() { return s.stop || s.next >= pieces || s.next < s.handed + (uint64_t)connections; });
                if (s.stop || s.next >= pieces) return;
                i = s.next++;
                p = &s.held[i];
                p->data.resize(size_of(i));
            }
            size_t      at = 0;
            std::string w;
            const bool  ok = fetch(
                offset + i * chunk, p->data.size(),
                [&](const uint8_t* data, size_t n) {
                    if (n > p->data.size() - at) return false;
                    std::memcpy(p->data.data() + at, data, n);
                    at += n;
                    {
                        const std::lock_guard<std::mutex> g(s.lock);
                        p->filled = at;
                    }
                    s.moved.notify_all();
                    return !s.stop;
                },
                &w, &s.stop);
            {
                const std::lock_guard<std::mutex> g(s.lock);
                if (ok && at == p->data.size()) p->done = true;
                else if (!s.stop) {
                    s.failed = true;
                    s.error = !ok && !w.empty() ? w : "the range came out short";
                    s.stop = true;
                }
            }
            s.moved.notify_all();
        }
    };
    std::vector<std::thread> threads;
    const uint64_t           count = std::min<uint64_t>((uint64_t)connections, pieces);
    threads.reserve((size_t)count);
    for (uint64_t k = 0; k < count; k++) threads.emplace_back(worker);

    std::string mine;
    bool        ok = true;
    size_t      sent = 0;   // of the piece handed on
    while (ok) {
        const uint8_t* from = nullptr;
        size_t         n = 0;
        bool           last = false;
        {
            std::unique_lock<std::mutex> l(s.lock);
            if (s.handed >= pieces) break;
            const uint64_t d = s.handed;
            auto           ready = [&]() {
                const auto it = s.held.find(d);
                return s.failed || (it != s.held.end() && (it->second.filled > sent || it->second.done));
            };
            // Woken by the connections, and now and then for the cancel.
            while (!ready() && !(cancel && *cancel)) s.moved.wait_for(l, std::chrono::milliseconds(100));
            if (cancel && *cancel) {
                mine = "cancelled";
                ok = false;
                break;
            }
            if (s.failed) {
                ok = false;
                break;
            }
            Piece& p = s.held[d];
            from = p.data.data() + sent;
            n = p.filled - sent;
            last = p.done && p.filled == p.data.size();
        }
        // The bytes up to `filled` are written: handed on without the lock.
        for (size_t at = 0; ok && at < n; at += SLICE)
            if (!sink(from + at, std::min(SLICE, n - at))) {
                mine = "stopped";
                ok = false;
            }
        sent += n;
        if (ok && last) {
            {
                const std::lock_guard<std::mutex> g(s.lock);
                s.held.erase(s.handed);
                s.handed++;
            }
            sent = 0;
            s.moved.notify_all();
        }
    }
    {
        const std::lock_guard<std::mutex> g(s.lock);
        s.stop = true;
    }
    s.moved.notify_all();
    for (std::thread& t : threads) t.join();
    if (!ok && why) *why = s.failed ? s.error : mine;
    return ok;
}

}   // namespace install
