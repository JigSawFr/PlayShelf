// source/lantern_app/peer_book.cpp and peer_service.cpp over loopback: the
// book (round trip, unknown keys kept, a newer schema left alone), a peer
// that answers a discovery datagram that asks for peers, serves a held file
// to a valid grant (whole, by range, HEAD), and refuses the rest: a grant
// for another file or server, a file it does not hold, a console that is off.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"
#include "lantern_app/peer_book.hpp"
#include "lantern_app/peer_service.hpp"
#include "util/paths.hpp"

using namespace lantern_app;

namespace
{
constexpr const char* SERVER_UID = "6f1c2a8e-4b7d-4e2a-9c31-8d5e2f0a7b14";
constexpr int64_t     NOW = 1'800'000'000;

class FixedClock final : public lantern::Clock
{
  public:
    int64_t unix_seconds() override { return NOW; }
};

std::string hex(const lantern::Digest& d)
{
    const auto h = lantern::to_hex(d);
    return std::string(h.data(), h.size());
}

std::string hex_key(const lantern::PublicKey& k)
{
    std::string s;
    char b[3];
    for (uint8_t v : k) {
        std::snprintf(b, sizeof b, "%02x", v);
        s += b;
    }
    return s;
}

// One request to the TCP port, the whole answer back.
std::string ask(uint16_t port, const std::string& request)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (::connect(fd, (sockaddr*)&a, sizeof a) != 0) return "connect failed";
    ::send(fd, request.data(), request.size(), 0);
    std::string out;
    char buffer[4096];
    ssize_t n;
    while ((n = ::recv(fd, buffer, sizeof buffer, 0)) > 0) out.append(buffer, (size_t)n);
    ::close(fd);
    return out;
}

std::string body_of(const std::string& answer)
{
    const size_t i = answer.find("\r\n\r\n");
    return i == std::string::npos ? std::string() : answer.substr(i + 4);
}

std::string get(const std::string& method, const std::string& hex_sha, const std::string& grant, const std::string& extra = "")
{
    return method + " /lantern/v1/blobs/sha256/" + hex_sha + " HTTP/1.1\r\nHost: x\r\nLantern-Grant: " + grant + "\r\n" + extra + "\r\n";
}
}   // namespace

int main()
{
    char tmpl[] = "/tmp/lantern_peers_serve_XXXXXX";
    const std::string root = ::mkdtemp(tmpl);
    ::mkdir((root + "/switch").c_str(), 0755);

    // A file this console holds.
    std::string content(100'000, '\0');
    for (size_t i = 0; i < content.size(); i++) content[i] = (char)(i * 31 + 7);
    const std::string file_path = root + "/switch/a.nro";
    {
        FILE* f = std::fopen(file_path.c_str(), "wb");
        std::fwrite(content.data(), 1, content.size(), f);
        std::fclose(f);
    }
    const lantern::Digest digest = lantern::Sha256::of(content);
    const lantern::Ed25519Seed seed{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    const lantern::PublicKey key = lantern::ed25519_public_key(seed);

    // ---- the book
    PeerBook book;
    const uint8_t random16[16] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6};
    book.set_uid(uuid_from(random16));
    book.set_server(SERVER_UID, hex_key(key));
    book.set_server("not-a-key", "zz");   // refused
    book.hold({hex(digest), content.size(), "/switch/a.nro"});
    book.hold({"nothex", 1, "/switch/b.nro"});   // refused
    CHECK(book.servers().size() == 1 && book.held().size() == 1);
    const std::string book_file = root + "/peers.json";
    CHECK(book.save(book_file));
    {
        // A key this version does not know stays when it writes.
        std::string text;
        paths::read_file(book_file, text);
        text.insert(text.rfind('}'), ",\"later\":42");
        FILE* f = std::fopen(book_file.c_str(), "wb");
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
        PeerBook again;
        CHECK(again.load(book_file));
        CHECK(again.uid() == book.uid() && again.held().size() == 1 && again.servers().count(SERVER_UID) == 1);
        CHECK(again.save(book_file));
        paths::read_file(book_file, text);
        CHECK(text.find("\"later\"") != std::string::npos);
        // A newer schema is read as empty and never written over.
        FILE* g = std::fopen(book_file.c_str(), "wb");
        std::fputs("{\"schema\":2,\"uid\":\"x\"}", g);
        std::fclose(g);
        PeerBook newer;
        CHECK(newer.load(book_file) && newer.newer && newer.uid().empty());
        CHECK(!newer.save(book_file));
    }

    // ---- the responder over its sockets
    FixedClock clock;
    PeerResponder responder(book, root, "Noor's Switch", clock);
    PeerServer server(responder, 0, 0);
    responder.set_enabled(true);
    CHECK(server.start().empty());
    const uint16_t tcp = server.tcp_port();

    // Discovery: only a datagram that asks for peers is answered.
    {
        const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(server.udp_port());
        ::inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        timeval tv{1, 0};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        const std::string_view plain = lantern::discovery_request(false);
        ::sendto(fd, plain.data(), plain.size(), 0, (sockaddr*)&a, sizeof a);
        char buffer[2048];
        CHECK(::recv(fd, buffer, sizeof buffer, 0) < 0);   // no answer to a plain request
        const std::string_view asks = lantern::discovery_request(true);
        ::sendto(fd, asks.data(), asks.size(), 0, (sockaddr*)&a, sizeof a);
        const ssize_t n = ::recv(fd, buffer, sizeof buffer, 0);
        CHECK(n > 0);
        if (n > 0) {
            const auto reply = lantern::read_peer_datagram(std::as_bytes(std::span(buffer, (size_t)n)));
            CHECK(reply && *reply);
            if (reply && *reply) {
                CHECK(std::string((*reply)->uid) == book.uid());
                CHECK((*reply)->port == tcp);
                CHECK(lantern::peer_serves(**reply, SERVER_UID));
            }
        }
        ::close(fd);
    }

    const std::string sha = hex(digest);
    const auto grant = lantern::make_grant(seed, SERVER_UID, digest, content.size(), NOW + 300);
    CHECK(grant.has_value());
    const std::string g(grant->begin(), grant->end());

    // The whole file.
    {
        const std::string a = ask(tcp, get("GET", sha, g));
        CHECK(a.rfind("HTTP/1.1 200", 0) == 0);
        CHECK(body_of(a) == content);
        CHECK(a.find("Accept-Ranges: bytes") != std::string::npos);
    }
    // A range.
    {
        const std::string a = ask(tcp, get("GET", sha, g, "Range: bytes=10-19\r\n"));
        CHECK(a.rfind("HTTP/1.1 206", 0) == 0);
        CHECK(body_of(a) == content.substr(10, 10));
    }
    // HEAD: headers, no body.
    {
        const std::string a = ask(tcp, get("HEAD", sha, g));
        CHECK(a.rfind("HTTP/1.1 200", 0) == 0 && body_of(a).empty());
    }
    // A grant that is not for this file, or not valid.
    {
        const lantern::Digest other = lantern::Sha256::of(std::string_view("other"));
        const auto wrong = lantern::make_grant(seed, SERVER_UID, other, 5, NOW + 300);
        const std::string a = ask(tcp, get("GET", sha, std::string(wrong->begin(), wrong->end())));
        CHECK(a.rfind("HTTP/1.1 403", 0) == 0);
        CHECK(a.find("WWW-Authenticate") == std::string::npos);
        CHECK(ask(tcp, get("GET", sha, "garbage")).rfind("HTTP/1.1 403", 0) == 0);
        const auto old = lantern::make_grant(seed, SERVER_UID, digest, content.size(), NOW - 10);
        CHECK(ask(tcp, get("GET", sha, std::string(old->begin(), old->end()))).rfind("HTTP/1.1 403", 0) == 0);
    }
    // A file it does not hold.
    {
        const lantern::Digest other = lantern::Sha256::of(std::string_view("other"));
        const auto ok = lantern::make_grant(seed, SERVER_UID, other, 5, NOW + 300);
        const std::string a = ask(tcp, get("GET", hex(other), std::string(ok->begin(), ok->end())));
        CHECK(a.rfind("HTTP/1.1 404", 0) == 0);
    }
    // The file changed on the card: no longer held whole.
    {
        FILE* f = std::fopen(file_path.c_str(), "ab");
        std::fputs("x", f);
        std::fclose(f);
        PeerResponder again(book, root, "n", clock);
        again.set_enabled(true);
        CHECK(again.answer_request(get("GET", sha, g), "127.0.0.1").head.rfind("HTTP/1.1 404", 0) == 0);
    }
    // Off: every request is 403, and no datagram is answered.
    {
        responder.set_enabled(false);
        CHECK(ask(tcp, get("GET", sha, g)).rfind("HTTP/1.1 403", 0) == 0);
        const std::string_view asks = lantern::discovery_request(true);
        CHECK(responder.answer_datagram(std::as_bytes(std::span(asks.data(), asks.size()))).empty());
    }
    server.stop();

    std::remove(file_path.c_str());
    std::remove(book_file.c_str());
    ::rmdir((root + "/switch").c_str());
    ::rmdir(root.c_str());
    CHECK_DONE("lantern_peers_serve");
}
