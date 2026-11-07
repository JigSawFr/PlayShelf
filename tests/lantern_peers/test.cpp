// source/lantern_app/peers.cpp and delta.cpp's reads from peers over a
// scripted network: a peer found by its datagram, the file from a peer that
// holds it, a peer that does not, one that gives wrong bytes, and the ranges
// of a differential update asked of a peer first.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "lantern_app/delta.hpp"
#include "lantern_app/peers.hpp"

using namespace lantern_app;

namespace
{
constexpr uint64_t SIZE = 600'000;
constexpr const char* UID = "6f1c2a8e-4b7d-4e2a-9c31-8d5e2f0a7b14";
constexpr const char* FILE_URL = "https://shelf.example.org/files/a.nro";
constexpr const char* INDEX_URL = "https://shelf.example.org/chunks/a.json";
constexpr std::int64_t NOW = 1'800'000'000;

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

const lantern::Ed25519Seed SEED = [] {
    lantern::Ed25519Seed s{};
    for (size_t i = 0; i < s.size(); i++) s[i] = (uint8_t)(i * 7 + 3);
    return s;
}();

struct Network final : lantern::Transport
{
    std::string body, index;
    std::string peer_body;                  // what peers serve (the file, unless a test changes it)
    std::vector<std::string> held_by;       // hosts that hold the file
    size_t server_reads = 0, peer_reads = 0;
    std::vector<std::string> peer_ranges;

    lantern::Result<void> send(const lantern::Request& r, lantern::ResponseHandler& h) override
    {
        const std::string url(r.url);
        if (url.ends_with("/.well-known/lantern")) {
            std::string key;
            lantern::String b64;
            const auto pub = lantern::ed25519_public_key(SEED);
            lantern::base64_encode(std::span<const std::uint8_t>(pub.data(), pub.size()), b64);
            key.assign(b64.data(), b64.size());
            return json(h, std::string("{\"schema\": 1, \"uid\": \"") + UID + "\", \"name\": \"Shelf\", \"base\": \"/api/lantern\", "
                           "\"versions\": [1], \"platforms\": [\"switch\"], \"account\": {\"state\": \"active\", \"scopes\": [\"library\"]}, "
                           "\"features\": {\"library\": 1, \"peers\": 1, \"deltas\": 1}, "
                           "\"peers\": {\"key\": \"" + key + "\", \"grant_lifetime\": 600}}");
        }
        if (url.find("/v1/grants/sha256/") != std::string::npos) {
            const auto digest = lantern::digest_from_hex(hex_of(body));
            const auto grant = lantern::make_grant(SEED, UID, *digest, body.size(), NOW + 300);
            return json(h, "{\"schema\": 1, \"sha256\": \"" + hex_of(body) + "\", \"size\": " + std::to_string(body.size()) +
                           ", \"grant\": \"" + std::string(grant->data(), grant->size()) + "\", \"expires\": \"2027-01-15T08:10:00Z\"}");
        }
        if (url == INDEX_URL) return json(h, index);
        if (url.find("/v1/blobs/sha256/") != std::string::npos) {
            const std::string host = url.substr(url.find("://") + 3, url.find(':', url.find("://") + 3) - url.find("://") - 3);
            bool holds = false;
            for (const auto& x : held_by) holds = holds || x == host;
            if (!holds) return answer(h, 404, "");
            if (r.method == lantern::Method::head) return answer(h, 200, "");
            peer_reads++;
            std::string range;
            for (const auto& hd : r.headers)
                if (hd.name == "Range") range = std::string(hd.value);
            peer_ranges.push_back(range);
            return serve(r, h, peer_body, range);
        }
        server_reads++;
        std::string range;
        for (const auto& hd : r.headers)
            if (hd.name == "Range") range = std::string(hd.value);
        return serve(r, h, body, range);
    }

  private:
    static lantern::Result<void> json(lantern::ResponseHandler& h, const std::string& text)
    {
        if (!h.on_status(200) || !h.on_header("Content-Type", "application/json")) return {};
        (void)h.on_body(bytes_of(text));
        return {};
    }
    static lantern::Result<void> answer(lantern::ResponseHandler& h, uint16_t status, const std::string& text)
    {
        if (!h.on_status(status)) return {};
        if (!text.empty()) (void)h.on_body(bytes_of(text));
        return {};
    }
    static lantern::Result<void> serve(const lantern::Request&, lantern::ResponseHandler& h, const std::string& from, const std::string& range)
    {
        uint64_t first = 0, last = from.size() - 1;
        if (range.starts_with("bytes=")) {
            char* dash = nullptr;
            first = std::strtoull(range.c_str() + 6, &dash, 10);
            last = std::strtoull(dash + 1, nullptr, 10);
            if (!h.on_status(206) ||
                !h.on_header("Content-Range", "bytes " + std::to_string(first) + "-" + std::to_string(last) + "/" + std::to_string(from.size())))
                return {};
        } else if (!h.on_status(200)) {
            return {};
        }
        (void)h.on_body(bytes_of(from).subspan((size_t)first, (size_t)(last - first + 1)));
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
    std::int64_t unix_seconds() override { return NOW; }
};

PeerAddress peer(const std::string& host, const std::string& uid)
{
    PeerAddress p;
    p.uid = uid;
    p.name = "Lea's Switch";
    p.host = host;
    p.base = "/lantern";
    p.port = 47116;
    p.tls = false;
    return p;
}

struct World
{
    Network network;
    Random random;
    Clock clock;
    lantern::ServerTrust trust;
    std::unique_ptr<lantern::Client> client;
    std::string out = "/tmp/playshelf-peers-test.bin";
    PackageRequest request;

    explicit World(const std::string& file)
    {
        network.body = network.peer_body = file;
        network.index = index_of(file);
        lantern::ClientConfig config;
        config.origin = "https://shelf.example.org";
        config.user_agent = "PlayShelf/test";
        config.platform = "switch";
        config.random = &random;
        config.clock = &clock;
        config.trust = &trust;
        auto made = lantern::Client::create(network, config);
        client = std::make_unique<lantern::Client>(std::move(*made));
        (void)client->handshake();
        request = {FILE_URL, file.size(), hex_of(file), {}};
        std::remove(out.c_str());
    }
    ~World() { std::remove(out.c_str()); }

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

    // fetch_from_peers on the file, written to `out`.
    bool from_peers(std::string* from = nullptr)
    {
        FILE* f = open_private(out, false);
        FileSink sink(f, nullptr, nullptr, 0);
        lantern::DownloadState state;
        state.reset();
        const lantern::DownloadTarget target{request.url, request.size, *lantern::digest_from_hex(request.sha256)};
        const bool ok = fetch_from_peers(*client, request, target, sink, state, nullptr, from);
        std::fclose(f);
        return ok;
    }
};

// The reply of a peer (SPEC.md 19.5), as it arrives.
struct Hear final : lantern::UdpSocket
{
    std::vector<std::string> replies;
    size_t next = 0;
    lantern::Result<void> send_to(std::string_view, std::uint16_t, std::span<const std::byte>) override { return {}; }
    lantern::Result<std::optional<lantern::Datagram>> receive(std::span<std::byte> buffer, std::chrono::milliseconds) override
    {
        if (next >= replies.size()) return std::optional<lantern::Datagram>{};
        const std::string& r = replies[next++];
        std::memcpy(buffer.data(), r.data(), r.size());
        lantern::Datagram d;
        d.size = r.size();
        const std::string host = "192.168.1." + std::to_string(10 + next);
        std::memcpy(d.address.data(), host.data(), host.size());
        d.address_size = host.size();
        d.port = 47115;
        return std::optional<lantern::Datagram>{d};
    }
};

std::string peer_reply(const std::string& uid, const std::string& served)
{
    return "{\"schema\": 1, \"lantern\": \"peer\", \"uid\": \"" + uid + "\", \"name\": \"Lea's Switch\", \"port\": 47116, "
           "\"base\": \"/lantern\", \"tls\": false, \"slots\": 2, \"servers\": [\"" + served + "\"]}";
}

void test_find()
{
    Hear air;
    air.replies = {peer_reply("11111111-1111-4111-8111-111111111111", UID),
                   peer_reply("22222222-2222-4222-8222-222222222222", "99999999-9999-4999-8999-999999999999"),   // another server's
                   peer_reply("33333333-3333-4333-8333-333333333333", UID)};
    const auto found = find_peers(air, UID);
    CHECK(found.size() == 2);   // the one that takes another server's grants is left out
    CHECK(found[0].uid == "11111111-1111-4111-8111-111111111111");
    CHECK(found[0].port == 47116 && found[0].base == "/lantern" && !found[0].tls);
    CHECK(found[0].host.starts_with("192.168.1."));
}

void test_from_peer()
{
    const std::string file = pseudo(SIZE, 3);
    World w(file);
    w.request.peers = {peer("192.168.1.11", "a"), peer("192.168.1.12", "b")};
    w.network.held_by = {"192.168.1.12"};   // the first does not hold it, the second does
    std::string from;
    CHECK(w.from_peers(&from));
    CHECK(w.written() == file);
    CHECK(w.network.server_reads == 0);   // nothing of the file came from the server
    CHECK(from == "Lea's Switch");        // and the result says whose it was
}

void test_no_peer_holds()
{
    const std::string file = pseudo(SIZE, 3);
    World w(file);
    w.request.peers = {peer("192.168.1.11", "a")};
    CHECK(!w.from_peers());   // the caller reads from the server
    CHECK(w.written().empty());
    CHECK(w.network.peer_reads == 0);
}

void test_wrong_bytes()
{
    const std::string file = pseudo(SIZE, 3);
    World w(file);
    w.request.peers = {peer("192.168.1.11", "a")};
    w.network.held_by = {"192.168.1.11"};
    w.network.peer_body = pseudo(SIZE, 4);   // a peer that serves other bytes of the right size
    CHECK(!w.from_peers());                  // refused by the file's SHA-256 (L-19.1-1)
    CHECK(w.written().empty());              // and nothing kept
}

void test_delta_from_peer()
{
    const std::string file = pseudo(SIZE, 3);
    std::string old = file;
    const std::string other = pseudo(50'000, 9);
    old.replace(300'000, other.size(), other);
    World w(file);
    w.network.held_by = {"192.168.1.11"};
    const std::vector<PeerAddress> peers = {peer("192.168.1.11", "a")};
    const std::vector<std::vector<std::byte>> seeds = {std::vector<std::byte>(bytes_of(old).begin(), bytes_of(old).end())};
    const DeltaResult r = fetch_delta(*w.client, w.request, INDEX_URL, w.network.index.size(), hex_of(w.network.index), seeds, w.out,
                                      nullptr, nullptr, peers);
    CHECK(r.ok);
    CHECK(w.written() == file);
    CHECK(r.from_seeds > 0);
    CHECK(w.network.peer_reads > 0);      // the ranges came from the peer
    CHECK(w.network.server_reads == 0);   // none from the server
}
}   // namespace

int main()
{
    test_find();
    test_from_peer();
    test_no_peer_holds();
    test_wrong_bytes();
    test_delta_from_peer();
    CHECK_DONE("lantern peers: peers found, a file read from one, differential ranges asked of it passed");
}
