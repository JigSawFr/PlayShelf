// What the system gives the Lantern client besides HTTP: random bytes from
// the platform's cryptographic generator, the time, and the UDP socket of
// LAN discovery (port 47115). Plain BSD sockets: the console's (libnx) are
// the same calls.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <lantern/discovery.hpp>
#include <lantern/http_signature.hpp>

namespace lantern_app
{

// getrandom() on a PC, csrngGetRandomBytes on the console.
class SystemRandom final : public lantern::RandomSource
{
  public:
    lantern::Result<void> fill(std::span<std::uint8_t> out) override;
};

class SystemClock final : public lantern::Clock
{
  public:
    std::int64_t unix_seconds() override;
};

// The broadcast address of an IPv4 network, from the address and the mask in
// network byte order ("192.168.1.255"); "" when there is no address. Some
// consoles drop 255.255.255.255, so discovery asks the network's own too.
std::string broadcast_address(std::uint32_t address, std::uint32_t mask);

// The broadcast address of the network this device is on, none without one:
// the console's through nifm, a PC's first IPv4 interface that is up.
std::optional<std::string> directed_broadcast();

// An IPv4 UDP socket that may send to a broadcast address.
class BroadcastSocket final : public lantern::UdpSocket
{
  public:
    static lantern::Result<BroadcastSocket> open();

    BroadcastSocket(const BroadcastSocket&)            = delete;
    BroadcastSocket& operator=(const BroadcastSocket&) = delete;
    BroadcastSocket(BroadcastSocket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    BroadcastSocket& operator=(BroadcastSocket&& other) noexcept;
    ~BroadcastSocket() override;

    lantern::Result<void> send_to(std::string_view address, std::uint16_t port, std::span<const std::byte> bytes) override;
    lantern::Result<std::optional<lantern::Datagram>> receive(std::span<std::byte> buffer, std::chrono::milliseconds timeout) override;

  private:
    explicit BroadcastSocket(int fd) : fd_(fd) {}
    int fd_ = -1;
};

}   // namespace lantern_app
