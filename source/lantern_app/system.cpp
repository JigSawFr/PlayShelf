// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/system.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <netinet/in.h>
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
#include <ifaddrs.h>
#include <net/if.h>
#endif
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __SWITCH__
#include <switch.h>
#elif !defined(__EMSCRIPTEN__)
#include <sys/random.h>
#endif

namespace lantern_app
{

lantern::Result<void> SystemRandom::fill(std::span<std::uint8_t> out)
{
#ifdef __SWITCH__
    if (R_FAILED(csrngInitialize()) || R_FAILED(csrngGetRandomBytes(out.data(), out.size())))
        return lantern::fail(lantern::Category::storage, "the console's random generator failed");
    return {};
#elif defined(__EMSCRIPTEN__)
    // getentropy() takes at most 256 bytes at a time.
    for (size_t at = 0; at < out.size(); at += 256)
        if (::getentropy(out.data() + at, std::min<size_t>(256, out.size() - at)) != 0)
            return lantern::fail(lantern::Category::storage, "the system's random generator failed");
    return {};
#else
    size_t at = 0;
    while (at < out.size()) {
        const ssize_t n = ::getrandom(out.data() + at, out.size() - at, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return lantern::fail(lantern::Category::storage, "the system's random generator failed");
        }
        at += (size_t)n;
    }
    return {};
#endif
}

std::int64_t SystemClock::unix_seconds() { return (std::int64_t)::time(nullptr); }

std::string broadcast_address(std::uint32_t address, std::uint32_t mask)
{
    if (address == 0) return {};
    in_addr to{};
    to.s_addr = (address & mask) | ~mask;
    char text[INET_ADDRSTRLEN] = {};
    if (!::inet_ntop(AF_INET, &to, text, sizeof text)) return {};
    return text;
}

std::optional<std::string> directed_broadcast()
{
#if defined(__SWITCH__)
    if (R_FAILED(nifmInitialize(NifmServiceType_User))) return std::nullopt;
    u32        address = 0, mask = 0, gateway = 0, dns1 = 0, dns2 = 0;
    const bool ok = R_SUCCEEDED(nifmGetCurrentIpConfigInfo(&address, &mask, &gateway, &dns1, &dns2)) && address != 0;
    nifmExit();
    if (!ok) return std::nullopt;
    const std::string text = broadcast_address(address, mask);
    if (text.empty()) return std::nullopt;
    return text;
#elif defined(__EMSCRIPTEN__)
    return std::nullopt;   // a page has no sockets of its own
#else
    ifaddrs* all = nullptr;
    if (::getifaddrs(&all) != 0) return std::nullopt;
    std::optional<std::string> found;
    for (const ifaddrs* it = all; it && !found; it = it->ifa_next) {
        if (!it->ifa_addr || !it->ifa_netmask || it->ifa_addr->sa_family != AF_INET) continue;
        if (!(it->ifa_flags & IFF_UP) || (it->ifa_flags & IFF_LOOPBACK) || !(it->ifa_flags & IFF_BROADCAST)) continue;
        const std::string text = broadcast_address(reinterpret_cast<const sockaddr_in*>(it->ifa_addr)->sin_addr.s_addr,
                                                   reinterpret_cast<const sockaddr_in*>(it->ifa_netmask)->sin_addr.s_addr);
        if (!text.empty()) found = text;
    }
    ::freeifaddrs(all);
    return found;
#endif
}

lantern::Result<BroadcastSocket> BroadcastSocket::open()
{
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return lantern::fail(lantern::Category::transport, "no UDP socket");
    const int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);
    return BroadcastSocket(fd);
}

BroadcastSocket& BroadcastSocket::operator=(BroadcastSocket&& other) noexcept
{
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_       = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

BroadcastSocket::~BroadcastSocket()
{
    if (fd_ >= 0) ::close(fd_);
}

lantern::Result<void> BroadcastSocket::send_to(std::string_view address, std::uint16_t port, std::span<const std::byte> bytes)
{
    char text[64];
    if (address.size() >= sizeof text) return lantern::fail(lantern::Category::usage, "not an IPv4 address");
    std::memcpy(text, address.data(), address.size());
    text[address.size()] = 0;
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port   = htons(port);
    if (::inet_pton(AF_INET, text, &to.sin_addr) != 1) return lantern::fail(lantern::Category::usage, "not an IPv4 address");
    if (::sendto(fd_, bytes.data(), bytes.size(), 0, reinterpret_cast<const sockaddr*>(&to), sizeof to) < 0)
        return lantern::fail(lantern::Category::transport, "the datagram was not sent");
    return {};
}

lantern::Result<std::optional<lantern::Datagram>> BroadcastSocket::receive(std::span<std::byte> buffer, std::chrono::milliseconds timeout)
{
    pollfd p{fd_, POLLIN, 0};
    int    ready = 0;
    do {
        ready = ::poll(&p, 1, (int)timeout.count());
    } while (ready < 0 && errno == EINTR);
    if (ready < 0) return lantern::fail(lantern::Category::transport, "the socket failed");
    if (ready == 0) return std::optional<lantern::Datagram>{};
    sockaddr_in from{};
    socklen_t   len = sizeof from;
    const ssize_t n = ::recvfrom(fd_, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&from), &len);
    if (n < 0) return lantern::fail(lantern::Category::transport, "the datagram was not received");
    lantern::Datagram d;
    d.size = (size_t)n;
    char text[INET_ADDRSTRLEN] = {};
    if (!::inet_ntop(AF_INET, &from.sin_addr, text, sizeof text)) return std::optional<lantern::Datagram>{};
    d.address_size = std::strlen(text);
    std::memcpy(d.address.data(), text, d.address_size);
    d.port = ntohs(from.sin_port);
    return std::optional<lantern::Datagram>{d};
}

}   // namespace lantern_app
