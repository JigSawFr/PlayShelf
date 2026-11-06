// A package fetched whole into a file on the SD card by the Lantern client
// (SPEC.md § 6.6): signed requests, the file's size and SHA-256 checked as it
// is written, resumed from what a file already holds. Only a file that is
// whole and verified is ever installed.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "lantern_app/platform.hpp"
#include "library/servers.hpp"

namespace lantern_app
{

struct PackageRequest
{
    std::string url;      // https://, an address of the server
    uint64_t    size = 0;
    std::string sha256;   // 64 hex digits
};

struct PackageResult
{
    bool        ok = false;
    bool        stopped = false;   // `stop` was set: the file keeps what was written
    std::string why;
};

// Fetches `request` from `server` into `path` (created, or taken up where it
// stands: its bytes are hashed again first). `progress(bytes)` is told as the
// file grows; `stop` ends the transfer. On success the file is exactly the
// package; on a size or SHA-256 that does not match it is emptied.
PackageResult fetch_package(Platform& platform, const library::Server& server, const PackageRequest& request,
                            const std::string& path, const std::atomic<bool>* stop,
                            const std::function<void(uint64_t)>& progress);

}   // namespace lantern_app
