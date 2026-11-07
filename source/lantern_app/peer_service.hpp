// A console that shares its files with the other consoles of its network
// (SPEC.md 19.4 and 19.5), off until its user turns it on.
//
// PeerResponder is the logic, with no networking: a discovery datagram that
// asks for peers gets the reply of 19.5, and a request for a blob goes to the
// SDK's PeerGate (grant, held files only, private addresses only, a rate per
// address), which answers with the headers and the part of the file to send.
// PeerServer is the two sockets that feed it: UDP 47115 and TCP, one
// connection at a time (so `slots` is 1), every read and write with a
// timeout. Host-tested over loopback (tests/lantern_peers_serve).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <lantern/lantern.hpp>

#include "lantern_app/peer_book.hpp"

namespace lantern_app
{

inline constexpr uint16_t PEER_PORT = 47116;
inline constexpr const char* PEER_BASE = "/lantern";

// What to send for one request.
struct PeerResponse
{
    std::string head;          // the status line, the headers and the empty line
    std::string body;          // a problem document, or empty
    std::string file;          // when set, the file to stream from the SD card ...
    uint64_t    offset = 0;    // ... [offset, offset + length)
    uint64_t    length = 0;
    bool        slot = false;  // give it back with release() once sent
};

class PeerResponder
{
  public:
    // `sd_root` is where the held files' paths start; `name` is the one the
    // reply gives (a person sees it).
    PeerResponder(const PeerBook& book, std::string sd_root, std::string name, lantern::Clock& clock);
    ~PeerResponder();
    PeerResponder(const PeerResponder&)            = delete;
    PeerResponder& operator=(const PeerResponder&) = delete;

    void set_enabled(bool enabled) { this->gate_->set_enabled(enabled); }
    // The port the reply gives (the server sets it once it listens).
    void set_port(uint16_t port) { this->port_ = port; }

    // The reply to a discovery datagram that asks for peers, "" otherwise
    // (a request that does not ask, bytes that are not one, a console that is
    // off, a console that holds no server's grant key).
    std::string answer_datagram(std::span<const std::byte> datagram) const;

    // `head` is the request as received up to the empty line; `remote` is the
    // client's address as text.
    PeerResponse answer_request(std::string_view head, std::string_view remote);
    void         release();

  private:
    class Held;
    std::string                    sd_root_, name_, uid_;
    uint16_t                       port_ = PEER_PORT;
    std::vector<std::string>       server_uids_;
    std::vector<lantern::GrantKey> keys_;
    std::unique_ptr<Held>          held_;
    std::unique_ptr<lantern::PeerGate> gate_;
    friend class PeerServer;
};

// The sockets. start() fails with a reason when a socket cannot be opened.
class PeerServer
{
  public:
    PeerServer(PeerResponder& responder, uint16_t udp_port, uint16_t tcp_port);
    ~PeerServer();
    PeerServer(const PeerServer&)            = delete;
    PeerServer& operator=(const PeerServer&) = delete;

    std::string start();   // "" when running, else why not
    void        stop();
    uint16_t    tcp_port() const { return this->tcp_port_; }
    uint16_t    udp_port() const { return this->udp_port_; }

  private:
    void run();
    void serve(int fd, const std::string& remote);

    PeerResponder&    responder_;
    uint16_t          udp_port_, tcp_port_;
    int               udp_ = -1, tcp_ = -1;
    std::atomic<bool> stop_{false};
    std::thread       thread_;
};

}   // namespace lantern_app
