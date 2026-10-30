// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/http_transport.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "util/http.hpp"

namespace library
{

Answer HttpTransport::get(const std::string& url, const std::vector<std::string>& headers, const std::string& pin)
{
    http::Response r;
    Answer         a;
    // A catalogue of tens of thousands of items: 32 MB at most.
    const bool ok = http::request(url, headers, pin, &r, &a.error, 32u << 20, 60);
    a.status = r.status;
    a.tls_failed = r.tls_failed;
    if (ok) a.body = std::move(r.body);
    // An answer >= 400 is no failure to the client: it reads the status.
    if (!ok && r.status >= 400) a.error.clear();
    return a;
}

Answer HttpTransport::call(const std::string& url, const http::Call& c, const std::vector<std::string>& headers,
                           const std::string& pin)
{
    http::Response r;
    Answer         a;
    // A file sent or written: no overall limit (http::call gives up on a
    // stall instead); else a minute, as get().
    const bool file = !c.file_path.empty() || !c.to_path.empty();
    const bool ok = http::call(url, c, headers, pin, &r, &a.error, 32u << 20, file ? 0 : 60);
    a.status = r.status;
    a.tls_failed = r.tls_failed;
    a.cookies = std::move(r.cookies);
    a.location = std::move(r.location);
    if (ok || r.status >= 400) a.body = std::move(r.body);
    if (!ok && r.status >= 400) a.error.clear();
    return a;
}

bool HttpTransport::reachable(const std::string& url)
{
    // Any answer, a TLS refusal included, says the host is there.
    http::Response r;
    std::string    error;
    http::request(url, {}, "", &r, &error, 64 * 1024, 3);
    return r.status != 0 || r.tls_failed;
}

std::string HttpTransport::certificate(const std::string& url, std::string* why)
{
    std::string pem;
    return http::peer_certificate(url, &pem, why) ? pem : std::string();
}

std::vector<Discovered> discover(const std::atomic<bool>* cancel, std::string* why)
{
    std::vector<Discovered> servers;
    const int               fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        if (why) *why = "no network for the discovery";
        return servers;
    }
    struct Closer
    {
        int fd;
        ~Closer() { ::close(fd); }
    } closer{fd};
    int         on = 1;
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (fd >= FD_SETSIZE || ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on) < 0 ||
        ::bind(fd, (sockaddr*)&local, sizeof local) < 0 || flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        if (why) *why = "the discovery socket cannot be set up";
        return servers;
    }
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(DISCOVERY_PORT);
    to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    bool       sent = false;
    int        sends = 0;
    while (Clock::now() - start < std::chrono::seconds(3)) {
        if (cancel && cancel->load()) return {};
        // Three broadcasts, 0.9 s apart: one may be lost.
        if (sends < 3 && Clock::now() - start >= std::chrono::milliseconds(900 * sends)) {
            const size_t n = std::strlen(DISCOVERY_REQUEST);
            if (::sendto(fd, DISCOVERY_REQUEST, n, 0, (sockaddr*)&to, sizeof to) == (ssize_t)n) sent = true;
            sends++;
        }
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(fd, &readable);
        timeval wait{0, 50000};
        const int ready = ::select(fd + 1, &readable, nullptr, nullptr, &wait);
        if (ready <= 0) continue;
        char        buf[DISCOVERY_MAX_REPLY + 1];   // one byte more: an answer too big is seen
        sockaddr_in from{};
        socklen_t   len = sizeof from;
        const ssize_t got = ::recvfrom(fd, buf, sizeof buf, 0, (sockaddr*)&from, &len);
        if (got <= 0 || from.sin_family != AF_INET) continue;
        char ip[INET_ADDRSTRLEN];
        if (!::inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip)) continue;
        if (const auto s = parse_discovery(std::string_view(buf, (size_t)got), ip)) add_discovered(servers, *s);
    }
    if (!sent && why) *why = "the discovery broadcast cannot be sent: is the console on a network?";
    return servers;
}

}   // namespace library
