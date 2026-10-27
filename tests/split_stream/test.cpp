// install/split_stream.cpp: a range over several connections handed on in
// order and whole, at most so many requests at once, one connection for a
// small range; a request failing, the sink refusing, a cancel: every
// request stopped, the reason given.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "install/split_stream.hpp"

#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

using namespace install;

namespace
{
std::vector<uint8_t> bytes(size_t n)
{
    std::vector<uint8_t> b(n);
    for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(i * 31 + (i >> 9));
    return b;
}

// A server in memory: each request sends its bytes in small, randomly late
// pieces; `fail_at`: the request covering that offset fails.
struct Server
{
    const std::vector<uint8_t>& body;
    uint64_t                    fail_at = UINT64_MAX;
    std::mutex                  lock;
    int                         now = 0, most = 0, requests = 0;

    explicit Server(const std::vector<uint8_t>& b) : body(b) {}

    RangeFetch fetch()
    {
        return [this](uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                      std::string* why, const std::atomic<bool>* stop) {
            {
                const std::lock_guard<std::mutex> g(lock);
                requests++;
                most = std::max(most, ++now);
            }
            std::mt19937 rng((unsigned)offset);
            bool         ok = true;
            for (uint64_t at = 0; ok && at < length;) {
                if (stop && *stop) {
                    if (why) *why = "cancelled";
                    ok = false;
                    break;
                }
                if (offset <= fail_at && fail_at < offset + length && at >= length / 2) {
                    if (why) *why = "Recv failure: Connection reset by peer";
                    ok = false;
                    break;
                }
                const uint64_t n = std::min<uint64_t>(length - at, 700 + rng() % 3000);
                std::this_thread::sleep_for(std::chrono::microseconds(rng() % 300));
                if (!sink(body.data() + offset + at, (size_t)n)) {
                    if (why) *why = "stopped";
                    ok = false;
                }
                at += n;
            }
            const std::lock_guard<std::mutex> g(lock);
            now--;
            return ok;
        };
    }
};

constexpr uint64_t CHUNK = 64 << 10;

void test_in_order()
{
    const auto body = bytes(1 << 20);
    for (int connections : { 1, 2, 4, 8, 12 }) {
        Server               srv(body);
        std::vector<uint8_t> got;
        std::string          why;
        // A range not on a chunk's edge, its last piece short.
        const uint64_t from = 1234, length = body.size() - 1234 - 777;
        CHECK(split_stream(from, length, connections, srv.fetch(), [&](const uint8_t* d, size_t n) {
            got.insert(got.end(), d, d + n);
            return true;
        }, &why, nullptr, CHUNK));
        CHECK(got == std::vector<uint8_t>(body.begin() + (long)from, body.begin() + (long)(from + length)));
        // Never more requests at once than connections (8 at most).
        CHECK(srv.most <= std::min(connections, MAX_CONNECTIONS));
        if (connections == 1) CHECK(srv.requests == 1);
        else CHECK(srv.requests == (int)((length + CHUNK - 1) / CHUNK));
    }
}

void test_small()
{
    // Less than two chunks: one request, as it is.
    const auto           body = bytes(50 << 10);
    Server               srv(body);
    std::vector<uint8_t> got;
    CHECK(split_stream(0, body.size(), 8, srv.fetch(), [&](const uint8_t* d, size_t n) {
        got.insert(got.end(), d, d + n);
        return true;
    }, nullptr, nullptr, CHUNK));
    CHECK(got == body && srv.requests == 1);
    CHECK(split_stream(0, 0, 4, srv.fetch(), [](const uint8_t*, size_t) { return false; }, nullptr));
}

void test_failures()
{
    const auto body = bytes(1 << 20);
    // A request failing: its reason, what came before it handed on in order.
    {
        Server srv(body);
        srv.fail_at = 600 << 10;
        std::vector<uint8_t> got;
        std::string          why;
        CHECK(!split_stream(0, body.size(), 4, srv.fetch(), [&](const uint8_t* d, size_t n) {
            got.insert(got.end(), d, d + n);
            return true;
        }, &why, nullptr, CHUNK));
        CHECK(why == "Recv failure: Connection reset by peer");
        // The failing piece (576 to 640 KiB) may have handed on some of its
        // bytes first, none past it.
        CHECK(got.size() < (640u << 10) && std::equal(got.begin(), got.end(), body.begin()));
        CHECK(srv.now == 0);   // every request ended
    }
    // The sink refusing (a pause): "stopped".
    {
        Server      srv(body);
        size_t      taken = 0;
        std::string why;
        CHECK(!split_stream(0, body.size(), 4, srv.fetch(), [&](const uint8_t*, size_t n) {
            taken += n;
            return taken < (300u << 10);
        }, &why, nullptr, CHUNK));
        CHECK(why == "stopped" && srv.now == 0);
    }
    // A cancel from outside: "cancelled".
    {
        Server            srv(body);
        std::atomic<bool> cancel{false};
        std::string       why;
        size_t            taken = 0;
        CHECK(!split_stream(0, body.size(), 4, srv.fetch(), [&](const uint8_t*, size_t n) {
            taken += n;
            if (taken > (200u << 10)) cancel = true;
            return true;
        }, &why, &cancel, CHUNK));
        CHECK(why == "cancelled" && srv.now == 0);
    }
}
}   // namespace

int main()
{
    test_in_order();
    test_small();
    test_failures();
    return CHECK_DONE("split stream: in order over 1 to 8 connections, a small range, a failure, a refusal and a cancel passed");
}
