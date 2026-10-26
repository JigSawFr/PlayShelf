// split_stream — a range of a package fetched over several connections at
// once and handed on in order: the range cut into CHUNK pieces, each taken
// by the next free connection (at most `connections` pieces held at once,
// the one handed on included); the first piece's bytes are handed on as they
// arrive, the others kept until their turn. For a server that serves each
// connection slowly: the install still sees one stream, its checkpoints and
// its pause unchanged.
//
// Plain C++ (std::thread) for the host tests (tests/split_stream).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace install
{

constexpr uint64_t SPLIT_CHUNK = 4u << 20;
constexpr int      MAX_CONNECTIONS = 8;

// One request: `length` bytes at `offset` given to `sink` as they arrive;
// false (why set) when it fails or `stop` is set meanwhile.
using RangeFetch = std::function<bool(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                                      std::string* why, const std::atomic<bool>* stop)>;

// `length` bytes at `offset`, over `connections` (1 to MAX_CONNECTIONS; a
// range of less than two chunks takes one), to `sink` in order. False when
// a request failed (its why), `sink` refused ("stopped") or `cancel` was set
// ("cancelled"); every request is stopped and waited for first.
bool split_stream(uint64_t offset, uint64_t length, int connections, const RangeFetch& fetch,
                  const std::function<bool(const uint8_t*, size_t)>& sink, std::string* why,
                  const std::atomic<bool>* cancel = nullptr, uint64_t chunk = SPLIT_CHUNK);

}   // namespace install
