// source/lantern/system.cpp: random bytes that are not all the same, a clock
// near now, and the UDP socket against a local socket that answers.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <arpa/inet.h>
#include <array>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "check.h"
#include "lantern/system.hpp"

using namespace lantern_app;

int main()
{
    SystemRandom random;
    std::array<uint8_t, 32> a{}, b{};
    CHECK(random.fill(a).has_value() && random.fill(b).has_value());
    const std::array<uint8_t, 32> zeros{};
    CHECK(a != b && a != zeros);

    SystemClock clock;
    CHECK(clock.unix_seconds() > 1'700'000'000);

    // A local socket that answers what it receives.
    const int server = ::socket(AF_INET, SOCK_DGRAM, 0);
    REQUIRE(server >= 0);
    sockaddr_in at{};
    at.sin_family      = AF_INET;
    at.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE(::bind(server, reinterpret_cast<sockaddr*>(&at), sizeof at) == 0);
    socklen_t len = sizeof at;
    REQUIRE(::getsockname(server, reinterpret_cast<sockaddr*>(&at), &len) == 0);

    auto socket = BroadcastSocket::open();
    REQUIRE(socket.has_value());
    const char hello[] = "ping";
    CHECK(socket->send_to("127.0.0.1", ntohs(at.sin_port), std::as_bytes(std::span(hello, 4))).has_value());
    char       got[16];
    sockaddr_in from{};
    socklen_t   flen = sizeof from;
    const ssize_t n  = ::recvfrom(server, got, sizeof got, 0, reinterpret_cast<sockaddr*>(&from), &flen);
    CHECK(n == 4 && std::string(got, 4) == "ping");
    CHECK(::sendto(server, "pong!", 5, 0, reinterpret_cast<sockaddr*>(&from), flen) == 5);

    std::array<std::byte, 64> buf{};
    auto reply = socket->receive(buf, std::chrono::milliseconds(1000));
    CHECK(reply && *reply && (*reply)->size == 5 && (*reply)->address_view() == "127.0.0.1");
    // Nothing more: a timeout is no datagram, not an error.
    auto none = socket->receive(buf, std::chrono::milliseconds(50));
    CHECK(none && !*none);
    CHECK(!socket->send_to("not-an-address", 1, std::as_bytes(std::span(hello, 4))).has_value());
    ::close(server);

    CHECK_DONE("lantern system: random, clock and the discovery socket passed");
}
