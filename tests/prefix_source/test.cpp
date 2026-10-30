// install/prefix_source.cpp: a package's first bytes from a file, the rest
// from its source, a range across both whole and in order; a missing file
// all from the source; the sink's refusal.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "install/prefix_source.hpp"
#include "temp_dir.hpp"

#include <algorithm>
#include <fstream>

using namespace install;

namespace
{
std::vector<uint8_t> bytes(size_t n)
{
    std::vector<uint8_t> b(n);
    for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(i * 7 + (i >> 11));
    return b;
}

// A package in memory that counts what it was asked for.
struct Counting : Source
{
    explicit Counting(const std::vector<uint8_t>& b) : body(b) {}
    const std::vector<uint8_t>& body;
    uint64_t                    asked = 0;
    uint64_t size() const override { return body.size(); }
    Done     read(uint64_t offset, uint8_t* out, size_t n) override
    {
        std::copy(body.begin() + (long)offset, body.begin() + (long)(offset + n), out);
        return {};
    }
    Done stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink) override
    {
        asked += length;
        if (!sink(body.data() + offset, (size_t)length)) return fail("stopped");
        return {};
    }
};

// Why a step failed; "" when it did not.
template <class T> std::string error_of(const std::expected<T, std::string>& r) { return r ? "" : r.error(); }

std::vector<uint8_t> all_of(Source& s, uint64_t offset, uint64_t length)
{
    std::vector<uint8_t> got;
    CHECK(s.stream(offset, length, [&](const uint8_t* d, size_t n) {
        got.insert(got.end(), d, d + n);
        return true;
    }));
    return got;
}
}   // namespace

int main()
{
    TempDir tmp("prefix_source", TempDir::Enter);
    const auto body = bytes(1 << 20);
    const uint64_t have = 600000;
    std::ofstream("ahead.part", std::ios::binary).write((const char*)body.data(), (long)have);

    {
        Counting     rest(body);
        PrefixSource s("ahead.part", have, rest);
        CHECK(s.size() == body.size());
        // Within the file: nothing asked of the source.
        CHECK(all_of(s, 1000, 5000) == std::vector<uint8_t>(body.begin() + 1000, body.begin() + 6000));
        CHECK(rest.asked == 0);
        // Across: the file's part, then the source's, whole.
        CHECK(all_of(s, have - 100, 300) == std::vector<uint8_t>(body.begin() + (long)have - 100, body.begin() + (long)have + 200));
        CHECK(rest.asked == 200);
        // Past it, and the whole package.
        CHECK(all_of(s, 0, body.size()) == body);
        uint8_t b[16];
        CHECK(s.read(have + 10, b, sizeof b) && std::equal(b, b + 16, body.begin() + (long)have + 10));
        // The sink refusing: stopped.
        const Done refused = s.stream(0, 4000, [](const uint8_t*, size_t) { return false; });
        CHECK(!refused && error_of(refused) == "stopped");
    }
    {
        // No file: all from the source.
        Counting     rest(body);
        PrefixSource s("missing.part", have, rest);
        CHECK(all_of(s, 0, 1000).size() == 1000 && rest.asked == 1000);
    }
    return CHECK_DONE("prefix source: from the file, across, from the source, no file, a refusal passed");
}
