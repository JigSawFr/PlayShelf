// A package fetched whole into a file on the SD card by the Lantern client
// (SPEC.md § 6.6), for the downloads fetched ahead: signed requests, the
// file's size and SHA-256 checked as it is written, resumed from what a file
// already holds.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lantern_app/peers.hpp"
#include "lantern_app/platform.hpp"
#include "library/servers.hpp"

namespace lantern_app
{

struct PackageRequest
{
    std::string url;      // https://, an address of the server
    uint64_t    size = 0;
    std::string sha256;   // 64 hex digits
    // Peers that may hold the file (SPEC.md 19.5): asked first, each for
    // the whole file or the chunks it holds, then the server. What a peer
    // gives is checked as anything else is; a peer that fails is passed by.
    std::vector<PeerAddress> peers;
};

struct PackageResult
{
    bool        ok = false;
    bool        stopped = false;   // `stop` was set: the file keeps what was written
    std::string why;
    std::string from_peer;   // the peer that gave the whole file, if one did (its name, else its address)
};

// A client of `server` for a package at `url`: made, handshaken, signed in;
// null with `why` when the url is not the server's, it is not the one that
// was paired, or the client could not be made.
std::unique_ptr<lantern::Client> open_client(Platform& platform, const library::Server& server, const std::string& url,
                                             std::string* why);

// Fetches `request` from `server` into `path` (created, or taken up where it
// stands: its bytes are hashed again first). `progress(bytes)` is told as the
// file grows; `stop` ends the transfer. On success the file is exactly the
// package; on a size or SHA-256 that does not match it is emptied.
PackageResult fetch_package(Platform& platform, const library::Server& server, const PackageRequest& request,
                            const std::string& path, const std::atomic<bool>* stop,
                            const std::function<void(uint64_t)>& progress);

// The package of `request`, put together from the chunks of `seed_files`
// that are already on the console and the ranges of the rest (SPEC.md
// 22.3), checked as `fetch_package` does. `chunks_url` is the package's
// chunk index, `chunks_size` and `chunks_sha256` its own. Without a usable
// seed or index, it reads the package whole, as `fetch_package` does. Seeds
// are held in memory: files over MAX_SEED_BYTES together are left out.
struct DeltaResult;

DeltaResult fetch_package_delta(Platform& platform, const library::Server& server, const PackageRequest& request,
                                const std::string& chunks_url, uint64_t chunks_size, const std::string& chunks_sha256,
                                const std::vector<std::string>& seed_files, const std::string& path,
                                const std::atomic<bool>* stop, const std::function<void(uint64_t)>& progress);

}   // namespace lantern_app
