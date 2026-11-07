// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/peer_service.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <aurore/extern/nlohmann/json.hpp>

namespace lantern_app
{

namespace
{
constexpr size_t MAX_HEAD = 8192;
constexpr int    IO_TIMEOUT_MS = 10'000;

const char* reason(int status)
{
    switch (status) {
        case 200: return "OK";
        case 206: return "Partial Content";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 416: return "Range Not Satisfiable";
        case 429: return "Too Many Requests";
        default: return "Error";
    }
}

std::string lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string trim(std::string_view v)
{
    while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.remove_prefix(1);
    while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r')) v.remove_suffix(1);
    return std::string(v);
}
}   // namespace

// The files this console holds whole: the book's, still on the SD card at the
// size they were checked at.
class PeerResponder::Held final : public lantern::HeldFiles
{
  public:
    Held(const PeerBook& book, std::string sd_root) : book_(book), root_(std::move(sd_root))
    {
        while (!root_.empty() && root_.back() == '/') root_.pop_back();
    }

    std::optional<uint64_t> size_of(const lantern::Digest& sha256) override
    {
        const auto hex = lantern::to_hex(sha256);
        const HeldFile* f = book_.find(std::string(hex.data(), hex.size()));
        if (!f) return std::nullopt;
        struct stat st {};
        if (::stat(path_of(*f).c_str(), &st) != 0 || !S_ISREG(st.st_mode) || (uint64_t)st.st_size != f->size) return std::nullopt;
        return f->size;
    }

    std::string path_of(const HeldFile& f) const { return root_ + f.path; }
    std::string path_for(const lantern::Digest& sha256) const
    {
        const auto hex = lantern::to_hex(sha256);
        const HeldFile* f = book_.find(std::string(hex.data(), hex.size()));
        return f ? path_of(*f) : std::string();
    }

  private:
    PeerBook book_;   // a copy: the book on disk changes under another thread
    std::string root_;
};

PeerResponder::PeerResponder(const PeerBook& book, std::string sd_root, std::string name, lantern::Clock& clock)
    : sd_root_(std::move(sd_root)), name_(std::move(name)), uid_(book.uid())
{
    for (const auto& [uid, key_hex] : book.servers()) {
        lantern::PublicKey key{};
        bool ok = key_hex.size() == 64;
        for (size_t i = 0; ok && i < 32; i++) {
            unsigned v = 0;
            ok = std::sscanf(key_hex.c_str() + 2 * i, "%2x", &v) == 1;
            key[i] = (uint8_t)v;
        }
        if (ok) this->server_uids_.push_back(uid);
        if (ok) this->keys_.push_back(lantern::GrantKey{{}, key});
    }
    // The views point into server_uids_, which no longer grows.
    for (size_t i = 0; i < this->keys_.size(); i++) this->keys_[i].uid = this->server_uids_[i];
    this->held_ = std::make_unique<Held>(book, this->sd_root_);
    lantern::PeerLimits limits;
    limits.slots = 1;
    limits.base  = PEER_BASE;
    this->gate_  = std::make_unique<lantern::PeerGate>(std::span<const lantern::GrantKey>(this->keys_), *this->held_, clock, limits);
    this->gate_->set_enabled(false);
}

PeerResponder::~PeerResponder() = default;

std::string PeerResponder::answer_datagram(std::span<const std::byte> datagram) const
{
    if (!this->gate_->enabled() || this->server_uids_.empty() || this->uid_.empty()) return {};
    if (datagram.size() > lantern::max_discovery_datagram) return {};
    const auto doc = nlohmann::json::parse(std::string_view((const char*)datagram.data(), datagram.size()), nullptr, false);
    if (!doc.is_object() || doc.value("lantern", std::string()) != "discover" || doc.value("peers", false) != true) return {};
    nlohmann::json reply = { { "schema", 1 },     { "lantern", "peer" },       { "uid", this->uid_ },
                             { "name", this->name_ }, { "port", this->port_ },   { "base", PEER_BASE },
                             { "tls", false },    { "slots", 1 },              { "servers", this->server_uids_ } };
    std::string text = reply.dump();
    if (text.size() > lantern::max_discovery_datagram) return {};
    return text;
}

PeerResponse PeerResponder::answer_request(std::string_view head, std::string_view remote)
{
    PeerResponse out;
    // The request line, then the three headers a peer reads.
    const size_t eol = head.find("\r\n");
    const std::string_view line = head.substr(0, eol);
    const size_t sp1 = line.find(' ');
    const size_t sp2 = sp1 == std::string_view::npos ? sp1 : line.find(' ', sp1 + 1);
    std::string method, target, grant, range, if_range;
    if (sp1 != std::string_view::npos && sp2 != std::string_view::npos) {
        method = std::string(line.substr(0, sp1));
        target = std::string(line.substr(sp1 + 1, sp2 - sp1 - 1));
    }
    size_t pos = eol == std::string_view::npos ? head.size() : eol + 2;
    while (pos < head.size()) {
        const size_t next = head.find("\r\n", pos);
        const std::string_view h = head.substr(pos, next == std::string_view::npos ? std::string_view::npos : next - pos);
        const size_t colon = h.find(':');
        if (colon != std::string_view::npos) {
            const std::string name = lower(trim(h.substr(0, colon)));
            if (name == "lantern-grant") grant = trim(h.substr(colon + 1));
            else if (name == "range") range = trim(h.substr(colon + 1));
            else if (name == "if-range") if_range = trim(h.substr(colon + 1));
        }
        if (next == std::string_view::npos) break;
        pos = next + 2;
    }

    lantern::PeerRequest request;
    request.method         = method;
    request.target         = target;
    request.grant          = grant;
    request.range          = range;
    request.if_range       = if_range;
    request.remote_address = remote;
    lantern::PeerAnswer answer = this->gate_->handle(request);

    out.head = "HTTP/1.1 " + std::to_string(answer.status) + " " + reason(answer.status) + "\r\n";
    for (const lantern::Header& h : answer.headers()) out.head += std::string(h.name) + ": " + std::string(h.value) + "\r\n";
    out.head += "Connection: close\r\n\r\n";
    out.body = std::string(answer.body);
    out.slot = answer.slot;
    if (answer.serve) {
        out.file   = this->held_->path_for(answer.sha256);
        out.offset = answer.offset;
        out.length = answer.length;
    }
    return out;
}

void PeerResponder::release() { this->gate_->release(); }

// ---- sockets ----------------------------------------------------------------

namespace
{
bool wait_fd(int fd, short events, int ms)
{
    pollfd p{fd, events, 0};
    return ::poll(&p, 1, ms) > 0 && (p.revents & events);
}

bool send_all(int fd, const char* data, size_t n)
{
    while (n > 0) {
        if (!wait_fd(fd, POLLOUT, IO_TIMEOUT_MS)) return false;
        const ssize_t w = ::send(fd, data, n, MSG_NOSIGNAL);
        if (w <= 0) {
            if (w < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            return false;
        }
        data += w;
        n -= (size_t)w;
    }
    return true;
}

int listen_on(uint16_t port, int type, uint16_t* bound)
{
    const int fd = ::socket(AF_INET, type, 0);
    if (fd < 0) return -1;
    const int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef SO_REUSEPORT
    if (type == SOCK_DGRAM) ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
#endif
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
    if (::bind(fd, (sockaddr*)&addr, sizeof addr) != 0 || (type == SOCK_STREAM && ::listen(fd, 4) != 0)) {
        ::close(fd);
        return -1;
    }
    socklen_t len = sizeof addr;
    if (bound && ::getsockname(fd, (sockaddr*)&addr, &len) == 0) *bound = ntohs(addr.sin_port);
    return fd;
}
}   // namespace

PeerServer::PeerServer(PeerResponder& responder, uint16_t udp_port, uint16_t tcp_port)
    : responder_(responder), udp_port_(udp_port), tcp_port_(tcp_port)
{
}

PeerServer::~PeerServer() { this->stop(); }

std::string PeerServer::start()
{
    if (this->thread_.joinable()) return {};
    this->udp_ = listen_on(this->udp_port_, SOCK_DGRAM, &this->udp_port_);
    if (this->udp_ < 0) return "could not listen for peers' questions";
    this->tcp_ = listen_on(this->tcp_port_, SOCK_STREAM, &this->tcp_port_);
    if (this->tcp_ < 0) {
        ::close(this->udp_);
        this->udp_ = -1;
        return "could not listen for peers' downloads";
    }
    this->responder_.set_port(this->tcp_port_);
    this->stop_ = false;
    this->thread_ = std::thread([this] { this->run(); });
    return {};
}

void PeerServer::stop()
{
    this->stop_ = true;
    if (this->thread_.joinable()) this->thread_.join();
    if (this->udp_ >= 0) ::close(this->udp_);
    if (this->tcp_ >= 0) ::close(this->tcp_);
    this->udp_ = this->tcp_ = -1;
}

void PeerServer::run()
{
    while (!this->stop_) {
        pollfd fds[2] = {{this->udp_, POLLIN, 0}, {this->tcp_, POLLIN, 0}};
        if (::poll(fds, 2, 200) <= 0) continue;
        if (fds[0].revents & POLLIN) {
            std::byte    buffer[lantern::max_discovery_datagram + 1];
            sockaddr_in  from{};
            socklen_t    len = sizeof from;
            const ssize_t n = ::recvfrom(this->udp_, buffer, sizeof buffer, 0, (sockaddr*)&from, &len);
            if (n > 0 && (size_t)n <= lantern::max_discovery_datagram) {
                const std::string reply = this->responder_.answer_datagram(std::span<const std::byte>(buffer, (size_t)n));
                if (!reply.empty()) ::sendto(this->udp_, reply.data(), reply.size(), 0, (sockaddr*)&from, len);
            }
        }
        if (fds[1].revents & POLLIN) {
            sockaddr_in from{};
            socklen_t   len = sizeof from;
            const int   fd = ::accept(this->tcp_, (sockaddr*)&from, &len);
            if (fd < 0) continue;
            char text[INET_ADDRSTRLEN] = {};
            ::inet_ntop(AF_INET, &from.sin_addr, text, sizeof text);
            this->serve(fd, text);
            ::close(fd);
        }
    }
}

void PeerServer::serve(int fd, const std::string& remote)
{
    std::string head;
    char        buffer[1024];
    while (head.find("\r\n\r\n") == std::string::npos) {
        if (head.size() > MAX_HEAD || !wait_fd(fd, POLLIN, IO_TIMEOUT_MS)) return;
        const ssize_t n = ::recv(fd, buffer, sizeof buffer, 0);
        if (n <= 0) return;
        head.append(buffer, (size_t)n);
    }
    PeerResponse r = this->responder_.answer_request(head, remote);
    bool ok = send_all(fd, r.head.data(), r.head.size());
    if (ok && !r.body.empty() && head.compare(0, 5, "HEAD ") != 0) ok = send_all(fd, r.body.data(), r.body.size());
    if (ok && !r.file.empty()) {
        FILE* f = std::fopen(r.file.c_str(), "rb");
        if (f && std::fseek(f, (long)r.offset, SEEK_SET) == 0) {
            uint64_t left = r.length;
            while (ok && left > 0) {
                const size_t want = (size_t)std::min<uint64_t>(left, sizeof buffer * 16);
                static thread_local std::vector<char> chunk(sizeof buffer * 16);
                const size_t got = std::fread(chunk.data(), 1, want, f);
                if (got == 0) break;
                ok = send_all(fd, chunk.data(), got);
                left -= got;
            }
        }
        if (f) std::fclose(f);
    }
    if (r.slot) this->responder_.release();
}

}   // namespace lantern_app
