// source/lantern_app/range_source.cpp over a scripted network that serves a
// package by Range: a read in the middle, one across the pieces, a stream, a
// sink that stops it, a read past the end, a server that does not honour Range.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "lantern_app/range_source.hpp"

using namespace lantern_app;

namespace
{
constexpr uint64_t SIZE = 1'300'000;   // more than two pieces

struct Server final : lantern::Transport
{
    std::string          body;
    bool                 honour = true;
    std::vector<std::string> ranges;

    Server()
    {
        body.resize(SIZE);
        for (size_t i = 0; i < body.size(); i++) body[i] = (char)((i * 131 + 17) >> 3);
    }

    lantern::Result<void> send(const lantern::Request& r, lantern::ResponseHandler& h) override
    {
        std::string range;
        for (const auto& hd : r.headers)
            if (hd.name == "Range") range = std::string(hd.value);
        ranges.push_back(range);
        uint64_t first = 0, last = SIZE - 1;
        if (range.starts_with("bytes=") && honour) {
            char* dash = nullptr;
            first = std::strtoull(range.c_str() + 6, &dash, 10);
            last = std::strtoull(dash + 1, nullptr, 10);
            if (!h.on_status(206) || !h.on_header("Content-Range", "bytes " + std::to_string(first) + "-" + std::to_string(last) + "/" + std::to_string(SIZE))) return {};
        } else if (!h.on_status(200)) {
            return {};
        }
        const std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(body.data()) + first, (size_t)(last - first + 1));
        (void)h.on_body(bytes);
        return {};
    }
};

struct Random final : lantern::RandomSource
{
    lantern::Result<void> fill(std::span<std::uint8_t> out) override
    {
        for (size_t i = 0; i < out.size(); i++) out[i] = (uint8_t)(i * 31 + 7);
        return {};
    }
};

struct Clock final : lantern::Clock
{
    std::int64_t unix_seconds() override { return 1'800'000'000; }
};

struct World
{
    Server     server;
    Random     random;
    Clock      clock;
    lantern::ServerTrust trust;
    RangeSource source;

    World() : source(make(server, random, clock, trust), {"https://shelf.example.org/files/a.nsp", SIZE, std::string(64, 'a'), {}}) {}

    static std::unique_ptr<lantern::Client> make(Server& s, Random& r, Clock& c, lantern::ServerTrust& t)
    {
        lantern::ClientConfig config;
        config.origin = "https://shelf.example.org";
        config.user_agent = "PlayShelf/test";
        config.platform = "switch";
        config.random = &r;
        config.clock = &c;
        config.trust = &t;
        auto client = lantern::Client::create(s, config);
        return std::make_unique<lantern::Client>(std::move(*client));
    }
};

void test_read()
{
    World w;
    CHECK(w.source.size() == SIZE);
    std::vector<uint8_t> out(1000);
    CHECK(w.source.read(123456, out.data(), out.size()));
    CHECK(std::memcmp(out.data(), w.server.body.data() + 123456, out.size()) == 0);
    CHECK(w.server.ranges.back() == "bytes=123456-124455");
    // The last bytes, and an empty read.
    CHECK(w.source.read(SIZE - 10, out.data(), 10));
    CHECK(std::memcmp(out.data(), w.server.body.data() + SIZE - 10, 10) == 0);
    const size_t asked = w.server.ranges.size();
    CHECK(w.source.read(5, out.data(), 0));
    CHECK(w.server.ranges.size() == asked);
    // Past the end: refused without asking.
    CHECK(!w.source.read(SIZE - 5, out.data(), 10));
    CHECK(w.server.ranges.size() == asked);
}

void test_stream()
{
    World w;
    std::string got;
    CHECK(w.source.stream(1000, SIZE - 2000, [&](const uint8_t* p, size_t n) {
        got.append(reinterpret_cast<const char*>(p), n);
        return true;
    }));
    CHECK(got == w.server.body.substr(1000, SIZE - 2000));
    CHECK(w.server.ranges.size() >= 3);   // pieces of at most 512 KiB

    size_t calls = 0;
    const auto stopped = w.source.stream(0, SIZE, [&](const uint8_t*, size_t) { return ++calls < 2; });
    CHECK(!stopped && stopped.error() == "stopped" && calls == 2);
}

void test_no_range()
{
    World w;
    w.server.honour = false;
    std::vector<uint8_t> out(100);
    // A whole file for a part of it is not taken.
    CHECK(!w.source.read(500, out.data(), out.size()));
}
}   // namespace

int main()
{
    test_read();
    test_stream();
    test_no_range();
    CHECK_DONE("lantern range: a package read by signed ranges passed");
}
