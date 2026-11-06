// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/crypto_bench.hpp"

#include <cstdint>
#include <cstdio>
#include <functional>

#include "util/aes.hpp"
#include "util/crypto_backend.hpp"
#include "util/sha256.hpp"

namespace crypto
{
namespace
{
constexpr size_t CHUNK = 64 * 1024;

// Bytes per second over `each`, in MB/s: `work` runs on one chunk at a time.
double measure(std::chrono::milliseconds each, const std::function<void(uint8_t*, size_t)>& work)
{
    std::vector<uint8_t> buf(CHUNK);
    for (size_t i = 0; i < buf.size(); i++)
        buf[i] = (uint8_t)(i * 31 + 7);
    const auto start = std::chrono::steady_clock::now();
    uint64_t   bytes = 0;
    do {
        work(buf.data(), buf.size());
        bytes += buf.size();
    } while (std::chrono::steady_clock::now() - start < each);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return seconds > 0 ? (double)bytes / seconds / 1e6 : 0;
}
}   // namespace

std::vector<BenchRow> benchmark(std::chrono::milliseconds each)
{
    const uint8_t key[32] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
    const uint8_t iv[16] = {};
    const aes::Aes128Ctr ctr(key, iv);
    uint64_t             offset = 0, sector = 0;
    Sha256               sha;

    struct Test
    {
        const char*                                   name;
        std::function<void(uint8_t*, size_t)>         work;
        bool                                          engine_has;
    };
    const Backend saved = backend();
    // What the engine has, whatever the setting says now.
    set_backend(Backend::Auto);
    const bool aes_hw = aes_engine() != nullptr, sha_hw = sha_engine() != nullptr;
    std::vector<Test> tests = {
        { "AES-CTR", [&](uint8_t* p, size_t n) { ctr.crypt({ p, n }, offset); offset += n; }, aes_hw },
        { "AES-XTS", [&](uint8_t* p, size_t n) { aes::aes128_xts_nintendo(key, { p, n }, sector, 0x200, true); sector += n / 0x200; }, aes_hw },
        { "SHA-256", [&](uint8_t* p, size_t n) { sha.update({ p, n }); }, sha_hw },
    };
    std::vector<BenchRow> rows;
    for (const Test& t : tests) {
        BenchRow row;
        row.name = t.name;
        set_backend(Backend::Software);
        row.software = measure(each, t.work);
        if (t.engine_has) {
            set_backend(Backend::Hardware);
            row.engine = measure(each, t.work);
        }
        rows.push_back(row);
    }
    set_backend(saved);
    return rows;
}

std::string bench_text(const std::vector<BenchRow>& rows)
{
    std::string out = status() + "\n";
    char        line[160];
    for (const BenchRow& r : rows) {
        if (r.engine < 0) {
            std::snprintf(line, sizeof(line), "\n%s: software %.1f MB/s (no hardware)", r.name.c_str(), r.software);
        } else {
            std::snprintf(line, sizeof(line), "\n%s: software %.1f MB/s, hardware %.1f MB/s (x%.1f)", r.name.c_str(), r.software, r.engine,
                          r.software > 0 ? r.engine / r.software : 0.0);
        }
        out += line;
    }
    return out;
}
}   // namespace crypto
