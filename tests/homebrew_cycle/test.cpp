// A homebrew app's whole life on the console over a scripted server: its
// first version read and installed, the next one put together from the copy
// on the SD card (the unchanged chunks stay, only the rest is read), the
// update written over it and the book kept, then the removal that leaves
// nothing behind.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "fs.hpp"
#include "homebrew/install.hpp"
#include "lantern_app/delta.hpp"
#include "temp_dir.hpp"
#include "util/sha256.hpp"

using namespace lantern_app;

namespace
{
constexpr uint64_t SIZE = 700'000;
constexpr const char* FILE_URL  = "https://shelf.example.org/files/demo.nro";
constexpr const char* INDEX_URL = "https://shelf.example.org/chunks/demo.json";
constexpr const char* NRO_PATH  = "/switch/org.example.demo/org.example.demo.nro";

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
    std::string body, index;
    uint64_t    read = 0;   // bytes of the file given

    lantern::Result<void> send(const lantern::Request& r, lantern::ResponseHandler& h) override
    {
        if (std::string(r.url) == INDEX_URL) {
            if (h.on_status(200)) (void)h.on_body(bytes_of(index));
            return {};
        }
        std::string range;
        for (const auto& hd : r.headers)
            if (hd.name == "Range") range = std::string(hd.value);
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
        read += last - first + 1;
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

lantern::Digest digest_of(const std::string& bytes)
{
    Sha256 h;
    h.update(bytes);
    return h.digest();
}

// The catalogue's entry for `file` as the version `version`.
lantern::Homebrew entry_for(const std::string& version, const std::string& file)
{
    lantern::Homebrew e;
    e.id = "org.example.demo";
    e.platform = "switch";
    e.kind = "app";
    e.name = "Demo";
    e.version = version.c_str();
    e.build = 1;
    e.package.format = "nro";
    e.package.file = "/files/demo.nro";
    e.package.size = file.size();
    e.package.sha256 = digest_of(file);
    e.package.path = NRO_PATH;
    lantern::ManifestEntry m;
    m.path = NRO_PATH;
    m.size = file.size();
    m.sha256 = e.package.sha256;
    e.package.manifest.push_back(m);
    e.package.manifest_sha256 = lantern::manifest_digest(e.package.manifest);
    return e;
}

struct World
{
    Server               server;
    Random               random;
    Clock                clock;
    lantern::ServerTrust trust;
    std::unique_ptr<lantern::Client> client;

    World()
    {
        lantern::ClientConfig config;
        config.origin = "https://shelf.example.org";
        config.user_agent = "PlayShelf/test";
        config.platform = "switch";
        config.random = &random;
        config.clock = &clock;
        config.trust = &trust;
        auto made = lantern::Client::create(server, config);
        client = std::make_unique<lantern::Client>(std::move(*made));
    }

    // The server now offers `file`; it is read into `path`, from `seeds`.
    DeltaResult fetch(const std::string& file, const std::vector<std::vector<std::byte>>& seeds, const std::string& path)
    {
        server.body = file;
        server.index = index_of(file);
        server.read = 0;
        const PackageRequest request{ FILE_URL, file.size(), hex_of(file), {} };
        return fetch_delta(*client, request, INDEX_URL, server.index.size(), hex_of(server.index), seeds, path, nullptr, nullptr);
    }
};

void test_life()
{
    TempDir     t("hb_cycle");
    const std::string sd = t.path() + "/sd", stage = t.path() + "/stage", pkg = t.path() + "/dl.nro";
    World w;

    // The first version: read whole (nothing on the console yet), installed.
    const std::string v1 = pseudo(SIZE, 1);
    const DeltaResult first = w.fetch(v1, {}, pkg);
    if (!CHECK(first.ok)) return;
    CHECK(first.from_seeds == 0 && w.server.read >= SIZE);
    const auto e1 = entry_for("1.0", v1);
    const auto done1 = homebrew::install(e1, pkg, sd, stage, nullptr, nullptr);
    if (!CHECK(done1.ok)) return;
    CHECK(fs::read(sd + NRO_PATH) == v1);

    // The next one: a stretch of the file changed. The copy on the SD card is
    // the seed, so most of it is not read again.
    std::string v2 = v1;
    const std::string other = pseudo(40'000, 99);
    v2.replace(300'000, other.size(), other);
    std::vector<std::byte> seed;
    CHECK(read_seed(sd + NRO_PATH, MAX_SEED_BYTES, &seed));
    const DeltaResult second = w.fetch(v2, { seed }, pkg);
    if (!CHECK(second.ok)) return;
    CHECK(second.from_seeds > SIZE / 2 && w.server.read < SIZE / 2);
    const auto e2 = entry_for("2.0", v2);
    const auto done2 = homebrew::install(e2, pkg, sd, stage, &done1.record, nullptr);
    if (!CHECK(done2.ok)) return;
    CHECK(fs::read(sd + NRO_PATH) == v2 && done2.removed.empty());

    // The removal leaves nothing of the app.
    CHECK(homebrew::uninstall(done2.record, sd).size() == 1);
    CHECK(!fs::exists(sd + "/switch/org.example.demo"));
}
}   // namespace

int main()
{
    test_life();
    CHECK_DONE("homebrew cycle: read, installed, updated from the copy on the card, removed");
}
