// What this console needs to serve files to its peers (SPEC.md 19.4),
// kept in config/playshelf/peers.json (schema 1): its own uid, the grant
// key of each server it paired with (the handshake's `peers.key`, read over
// the pinned connection), and the files it holds whole, each with the size
// and SHA-256 it checked at install time. A file of a newer schema is read
// as empty and never written over; keys this version does not know are kept
// when it writes. Plain C++, host-tested (tests/lantern_peer_book).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <lantern/lantern.hpp>

namespace lantern_app
{

struct HeldFile
{
    std::string sha256;   // 64 lower-case hex digits
    uint64_t    size = 0;
    std::string path;     // from the SD root, "/switch/x/x.nro"
};

class PeerBook
{
  public:
    static constexpr size_t MAX_SERVERS = 32;
    static constexpr size_t MAX_HELD = 512;

    bool load(const std::string& path);
    bool save(const std::string& path) const;

    // This console's own uid as a peer (a random UUID, made on first use).
    const std::string& uid() const { return this->uid_; }
    void               set_uid(const std::string& uid) { this->uid_ = uid; }

    // The grant key (64 hex digits) of the server of `server_uid`.
    void                       set_server(const std::string& server_uid, const std::string& key_hex);
    const std::map<std::string, std::string>& servers() const { return this->servers_; }

    // Holds the file of `sha256` at `path`; a file at the same path is replaced.
    void                  hold(const HeldFile& file);
    void                  release(const std::string& path);
    const std::vector<HeldFile>& held() const { return this->held_; }
    const HeldFile*       find(const std::string& sha256) const;

    bool newer = false;   // the file on disk is of a later schema: not written

  private:
    std::string                        uid_;
    std::map<std::string, std::string> servers_;
    std::vector<HeldFile>              held_;
    std::string                        extra_;   // the keys of the file this version does not know (a JSON object)
};

// A random version-4 UUID from `bytes` (16 random bytes).
std::string uuid_from(const uint8_t bytes[16]);

}   // namespace lantern_app
