// install/ivfc.cpp: the hash tree built over data of every size (empty,
// shorter than a block, exact multiples, several levels deep), laid out as
// the header says, written to and read back from a file system header, and
// verified; and every kind of damage found: a data byte, a hash at each
// level, the master hash, a level that cannot be read, a header of another
// shape.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>
#include <random>

#include "check.h"
#include "install/ivfc.hpp"

using namespace install;
using Bytes = std::vector<uint8_t>;

namespace
{
Bytes noise(size_t n, uint32_t seed)
{
    std::mt19937 rng(seed);
    Bytes        b(n);
    for (auto& x : b) x = (uint8_t)rng();
    return b;
}

struct Section
{
    Bytes        bytes;   // data then hash levels
    ivfc::Layout layout;
};

Section build(const Bytes& data, size_t piece)
{
    ivfc::Builder b;
    for (size_t i = 0; i < data.size(); i += piece)
        b.add({data.data() + i, std::min(piece, data.size() - i)});
    auto r = b.finish();
    Section s;
    s.bytes = data;
    s.bytes.insert(s.bytes.end(), r.hash_levels.begin(), r.hash_levels.end());
    s.layout = r.layout;
    return s;
}

ivfc::ReadFn reader(const Bytes& b)
{
    return [&b](uint64_t off, std::span<uint8_t> out) {
        if (off > b.size() || out.size() > b.size() - off) return false;
        std::memcpy(out.data(), b.data() + off, out.size());
        return true;
    };
}

void test_layouts()
{
    for (size_t n : {size_t(1), size_t(100), size_t(0x3fff), size_t(0x4000), size_t(0x4001), size_t(0x8000), size_t(0x123456), size_t(5u << 20)}) {
        for (size_t piece : {size_t(1) << 20, size_t(777), size_t(0x4000)}) {
            if (n > (1u << 20) && piece < 4096) continue;   // slow and no more telling
            const Bytes data = noise(n, (uint32_t)n);
            Section     s = build(data, piece);
            const auto& l = s.layout;
            CHECK(l.offset[5] == 0 && l.size[5] == n);
            uint64_t at = n;
            for (size_t i = 0; i < 5; i++) {
                CHECK(l.offset[i] == at);
                CHECK(l.size[i] % ivfc::BLOCK == 0 && l.size[i] > 0);
                at += l.size[i];
            }
            CHECK(at == s.bytes.size());
            CHECK(s.bytes.size() - n == ivfc::hash_levels_size(n));
            CHECK(ivfc::verify(l, reader(s.bytes)).has_value());

            uint8_t fs[0x200] = {};
            ivfc::write_fs_header(fs, l);
            auto back = ivfc::read_fs_header(fs);
            if (CHECK(back.has_value())) {
                CHECK(back->offset == l.offset && back->size == l.size && back->order == l.order && back->master == l.master);
            }
        }
    }
}

void test_pieces_agree()
{
    const Bytes data = noise(300'000, 3);
    const auto  a = build(data, 1 << 20), b = build(data, 1), c = build(data, 12345);
    CHECK(a.bytes == b.bytes && a.bytes == c.bytes);
    CHECK(a.layout.master == b.layout.master);
}

void test_damage()
{
    const Bytes data = noise(200'000, 5);
    const auto  s = build(data, 1 << 20);
    CHECK(ivfc::verify(s.layout, reader(s.bytes)).has_value());
    auto broken = [&](const char* what, size_t at) {
        Bytes b = s.bytes;
        b[at] ^= 1;
        const bool refused = !ivfc::verify(s.layout, reader(b)).has_value();
        if (!refused) fprintf(stderr, "not refused: %s\n", what);
        CHECK(refused);
    };
    broken("first data byte", 0);
    broken("last data byte", data.size() - 1);
    broken("a middle data byte", 100'000);
    for (size_t i = 0; i < 5; i++)   // a hash of each level (the first hash; the top level is the master's)
        broken("a hash", s.layout.offset[i]);
    {
        auto l = s.layout;
        l.master[0] ^= 1;
        CHECK(!ivfc::verify(l, reader(s.bytes)).has_value());
    }
    {   // a read that fails
        const Bytes cut(s.bytes.begin(), s.bytes.begin() + 150'000);
        CHECK(!ivfc::verify(s.layout, reader(cut)).has_value());
    }
    {   // a hash level too small for its level
        auto l = s.layout;
        l.size[4] = 32;
        CHECK(!ivfc::verify(l, reader(s.bytes)).has_value());
    }
}

void test_header_refusals()
{
    const Bytes data = noise(50'000, 7);
    const auto  s = build(data, 1 << 20);
    uint8_t     fs[0x200] = {};
    ivfc::write_fs_header(fs, s.layout);
    CHECK(ivfc::read_fs_header(fs).has_value());
    auto refuses = [&](const char* what, auto&& edit) {
        uint8_t t[0x200];
        std::memcpy(t, fs, sizeof t);
        edit(t);
        const bool r = !ivfc::read_fs_header(t).has_value();
        if (!r) fprintf(stderr, "not refused: %s\n", what);
        CHECK(r);
    };
    refuses("magic", [](uint8_t* t) { t[8] = 'X'; });
    refuses("version", [](uint8_t* t) { t[0x0e] = 3; });
    refuses("hash size", [](uint8_t* t) { t[0x10] = 0x10; });
    refuses("layers", [](uint8_t* t) { t[0x14] = 5; });
    refuses("block order small", [](uint8_t* t) { t[0x18 + 0x10] = 3; });
    refuses("block order big", [](uint8_t* t) { t[0x18 + 0x18 * 3 + 0x10] = 30; });
    refuses("offset huge", [](uint8_t* t) { t[0x18 + 0x18 * 2 + 7] = 0x7f; });
}
}   // namespace

int main()
{
    test_layouts();
    test_pieces_agree();
    test_damage();
    test_header_refusals();
    CHECK_DONE("ivfc: ok");
}
