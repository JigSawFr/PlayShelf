// crypto_bench — how fast the AES modes and SHA-256 run on the portable code
// and on the engine of util/crypto_backend, for the Developer screen: each
// test works on a buffer for a set time and says MB/s. The setting is put
// back afterwards; the results are the same bytes either way, so an install
// running meanwhile only slows the measure.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace crypto
{
struct BenchRow
{
    std::string name;
    double      software = 0;   // MB/s (10^6 bytes per second)
    double      engine = -1;   // negative: no engine, or it did not pass its check
};

std::vector<BenchRow> benchmark(std::chrono::milliseconds each);
// The rows as lines of text, with the engine's status first.
std::string bench_text(const std::vector<BenchRow>& rows);
}   // namespace crypto
