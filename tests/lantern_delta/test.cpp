// source/lantern_app/delta.cpp's fetch_delta over a scripted network: a
// package put together from an older copy and the ranges that differ, an
// index that cannot be used (read whole), and a seed that matches nothing.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "check.h"
#include "lantern_app/delta.hpp"

using namespace lantern_app;

namespace
{
constexpr uint64_t SIZE = 900'000;
constexpr const char* FILE_URL  = "https://shelf.example.org/files/a.nro";
constexpr const char* INDEX_URL = "https://shelf.example.org/chunks/a.json";

std::string pseudo(uint64_t n, uint32_t salt)
{
    std::string s(n, '\0');
    uint32_t x = salt * 2654435761u + 1;
    for (auto& c : s) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        c = (char)(x >> 11);
    }
    return s;
}

std::span<const std::byte> bytes_of(const std::string& s)
{
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

std::string hex_of(const std::string& s)
{
    lantern::Sha256 sha;
    sha.update(bytes_of(s));
    return std::string(lantern::to_hex(sha.finish()).data(), 64);
}

// The index of `file`, as a server writes it (SPEC.md 22.1).
std::string index_of(const std::string& file)
{
    const auto cuts = lantern::cdc_cut(bytes_of(file), 65536);
    std::string out = "{\"chunker\": {\"alg\": \"lantern-cdc-1\", \"size\": 65536}, \"chunks\": [";
    bool first = true;
    for (const auto& c : *cuts) {
        out += std::string(first ? "" : ", ") + "[" + std::to_string(c.length) + ", \"" + hex_of(file.substr(c.offset, c.length)) + "\"]";
        first = false;
    }
    return out + "], \"schema\": 1, \"sha256\": \"" + hex_of(file) + "\", \"size\": " + std::to_string(file.size()) + "}";
}

struct Server final : lantern::Transport
{
    std::string              body, index;
    std::vector<std::string> ranges;
    bool                     index_ok = true;

    lantern::Result<void> send(const lantern::Request& r, lantern::ResponseHandler& h) override
    {
        const std::string url(r.url);
        if (url == INDEX_URL) {
            if (!index_ok || !h.on_status(200)) return {};
            (void)h.on_body(bytes_of(index));
            return {};
        }
        std::string range;
        for (const auto& hd : r.headers)
            if (hd.name == "Range") range = std::string(hd.value);
        ranges.push_back(range);
        uint64_t first = 0, last = body.size() - 1;
        if (range.starts_with("bytes=")) {
            char* dash = nullptr;
            first = std::strtoull(range.c_str() + 6, &dash, 10);
            last = std::strtoull(dash + 1, nullptr, 10);
            if (!h.on_status(206) ||
                !h.on_header("Content-Range", "bytes " + std::to_string(first) + "-" + std::to_string(last) + "/" + std::to_string(body.size())))
                return {};
        } else if (!h.on_status(200)) {
            return {};
        }
        (void)h.on_body(bytes_of(body).subspan((size_t)first, (size_t)(last - first + 1)));
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
    Server               server;
    Random               random;
    Clock                clock;
    lantern::ServerTrust trust;
    std::unique_ptr<lantern::Client> client;
    std::string          out = "/tmp/playshelf-delta-test.bin";
    PackageRequest       request;

    World(const std::string& file)
    {
        server.body = file;
        server.index = index_of(file);
        lantern::ClientConfig config;
        config.origin = "https://shelf.example.org";
        config.user_agent = "PlayShelf/test";
        config.platform = "switch";
        config.random = &random;
        config.clock = &clock;
        config.trust = &trust;
        auto made = lantern::Client::create(server, config);
        client = std::make_unique<lantern::Client>(std::move(*made));
        request = {FILE_URL, file.size(), hex_of(file)};
        std::remove(out.c_str());
    }
    ~World() { std::remove(out.c_str()); }

    DeltaResult run(const std::vector<std::vector<std::byte>>& seeds)
    {
        return fetch_delta(*client, request, INDEX_URL, server.index.size(), hex_of(server.index), seeds, out, nullptr, nullptr);
    }
    std::string written() const
    {
        std::string s;
        if (FILE* f = std::fopen(out.c_str(), "rb")) {
            char buf[8192];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
            std::fclose(f);
        }
        return s;
    }
};

std::vector<std::vector<std::byte>> seed_of(const std::string& s)
{
    return {std::vector<std::byte>(bytes_of(s).begin(), bytes_of(s).end())};
}

// An older version: the same file with a stretch of it changed.
std::string older(const std::string& file)
{
    std::string s = file;
    const std::string other = pseudo(60'000, 99);
    s.replace(400'000, other.size(), other);
    return s;
}

void test_changed_stretch()
{
    const std::string file = pseudo(SIZE, 1);
    World w(file);
    const DeltaResult r = w.run(seed_of(older(file)));
    CHECK(r.ok);
    CHECK(!r.whole);
    CHECK(r.from_seeds > SIZE / 2);                 // most of it was already there
    CHECK(r.from_ranges > 0 && r.from_ranges < SIZE / 2);
    CHECK(w.written() == file);
}

void test_unrelated_seed()
{
    const std::string file = pseudo(SIZE, 1);
    World w(file);
    const DeltaResult r = w.run(seed_of(pseudo(200'000, 7)));
    CHECK(r.ok);   // nothing to take from the seed: the ranges give it all
    CHECK(r.from_seeds == 0);
    CHECK(w.written() == file);
}

void test_index_not_usable()
{
    const std::string file = pseudo(SIZE, 1);
    World w(file);
    w.server.index_ok = false;   // the server does not give the index
    const DeltaResult r = w.run(seed_of(older(file)));
    CHECK(!r.ok);
    CHECK(r.whole);          // the caller reads the file whole
    CHECK(r.why.empty());
    CHECK(w.server.ranges.empty());
}

void test_wrong_package()
{
    const std::string file = pseudo(SIZE, 1);
    World w(file);
    w.request.sha256 = std::string(64, '0');   // the catalogue's checksum is not the file's
    const DeltaResult r = w.run(seed_of(older(file)));
    CHECK(!r.ok);
    CHECK(w.written().empty());   // the file is emptied, not left half true
}
}   // namespace

int main()
{
    test_changed_stretch();
    test_unrelated_seed();
    test_index_not_usable();
    test_wrong_package();
    CHECK_DONE("lantern delta: a package put together from an older copy passed");
}
