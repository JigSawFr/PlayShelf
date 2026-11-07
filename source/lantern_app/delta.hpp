// A package put together from an older copy the console already holds and the
// ranges of it that differ (SPEC.md 22.3).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <lantern/lantern.hpp>

#include "lantern_app/file_sink.hpp"
#include "lantern_app/package.hpp"

namespace lantern_app
{

// What a read from seeds took, for a message to the user.
struct DeltaResult : PackageResult
{
    uint64_t from_seeds = 0;    // bytes taken from the files already on the console
    uint64_t from_ranges = 0;   // bytes read from the server
    bool     whole = false;     // it was read whole
};

// Seeds are held in memory: files over this together are left out.
inline constexpr uint64_t MAX_SEED_BYTES = 48u << 20;

// `file` read whole into memory, unless it is missing, empty or over `room`.
bool read_seed(const std::string& file, uint64_t room, std::vector<std::byte>* out);

// The whole file from a peer of `request.peers` that holds it, if one does:
// each is asked with this file's grant and nothing else of the server's
// (SPEC.md 19.1). A peer that does not hold it, fails, or gives what does
// not check is passed by, the sink emptied for the next. False when none
// gave it: the caller reads from the server.
bool fetch_from_peers(lantern::Client& client, const PackageRequest& request, const lantern::DownloadTarget& target,
                      FileSink& sink, lantern::DownloadState& state, const std::atomic<bool>* stop);

// The package of `request` into `path`, from the chunks `held` has and the
// ranges of the rest, over a client already made. `chunks_url` is the
// package's chunk index, `chunks_size` and `chunks_sha256` its own. When the
// index cannot be used nothing is written: `whole` is set, `ok` and `why` are
// not, and the caller reads the file whole (SPEC.md L-22.3-4).
DeltaResult fetch_delta(lantern::Client& client, const PackageRequest& request, const std::string& chunks_url,
                        uint64_t chunks_size, const std::string& chunks_sha256,
                        const std::vector<std::vector<std::byte>>& held, const std::string& path,
                        const std::atomic<bool>* stop, const std::function<void(uint64_t)>& progress,
                        const std::vector<PeerAddress>& peers = {});

}   // namespace lantern_app
