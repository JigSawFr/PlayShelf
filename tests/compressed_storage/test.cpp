// install/compressed_storage.cpp: a RomFS (some blocks that compress, some
// that do not, some zeros) cut in blocks with liblz4 and read back whole and
// by ranges; the tables of several sizes (one entry set, several, a stored
// run merged); and what is refused: damaged node and entry fields, entries
// out of order or outside the data, kinds and sizes the firmware does not
// read, a codec that lies. Self-consistency only: whether the firmware reads
// what is built here takes a console.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <lz4.h>
#include <lz4hc.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>

#include "check.h"
#include "install/compressed_storage.hpp"

using namespace install;
using namespace install::cstore;
using Bytes = std::vector<uint8_t>;

namespace
{
std::optional<size_t> lz4_compress(std::span<const uint8_t> in, std::span<uint8_t> out)
{
    int n = LZ4_compress_HC((const char*)in.data(), (char*)out.data(), (int)in.size(), (int)out.size(), 9);
    if (n <= 0) return std::nullopt;
    return (size_t)n;
}

bool lz4_decompress(std::span<const uint8_t> in, std::span<uint8_t> out)
{
    return LZ4_decompress_safe((const char*)in.data(), (char*)out.data(), (int)in.size(), (int)out.size()) == (int)out.size();
}

// A RomFS of `blocks` blocks of `bs`: runs of text-like bytes (they compress),
// then random ones (they do not), then zeros, repeating.
Bytes image(size_t size, uint32_t seed)
{
    std::mt19937 rng(seed);
    Bytes        b(size);
    for (size_t i = 0; i < size;) {
        const size_t run = std::min<size_t>(size - i, 1000 + rng() % 20000);
        switch (rng() % 3) {
        case 0: for (size_t k = 0; k < run; k++) b[i + k] = (uint8_t)("abcdefgh"[(k / 3) % 8]); break;
        case 1: for (size_t k = 0; k < run; k++) b[i + k] = (uint8_t)rng(); break;
        default: std::memset(&b[i], 0, run); break;
        }
        i += run;
    }
    return b;
}

struct Made
{
    Bytes    data, table;
    uint32_t entries = 0;
    uint64_t virt = 0;
    bool     ok = false;
    std::string err;
};

Made build(const Bytes& img, uint32_t bs, size_t piece, CompressFn c = lz4_compress, DecompressFn v = lz4_decompress)
{
    Made m;
    Builder b(bs, c, v, [&](std::span<const uint8_t> s) {
        m.data.insert(m.data.end(), s.begin(), s.end());
        return true;
    });
    for (size_t i = 0; i < img.size(); i += piece)
        if (!b.add({img.data() + i, std::min(piece, img.size() - i)})) break;
    m.ok = b.finish();
    m.err = b.error();
    if (m.ok) {
        m.table = b.table();
        m.entries = b.entry_count();
        m.virt = b.virtual_size();
        CHECK(b.data_size() == m.data.size());
    }
    return m;
}

Parsed<Reader> open(const Made& m, uint64_t data_size = ~0ull)
{
    return Reader::open(m.table, (int32_t)m.entries, data_size == ~0ull ? m.data.size() : data_size, lz4_decompress);
}

bool read_all(const Reader& r, const Made& m, Bytes& out)
{
    out.assign(r.virtual_size(), 0);
    return r.read(0, out, [&](uint64_t off, std::span<uint8_t> o) {
        if (off > m.data.size() || o.size() > m.data.size() - off) return false;
        std::memcpy(o.data(), m.data.data() + off, o.size());
        return true;
    });
}

void roundtrip(size_t size, uint32_t bs, size_t piece, uint32_t seed)
{
    const Bytes img = image(size, seed);
    Made        m = build(img, bs, piece);
    if (!CHECK(m.ok)) return;
    CHECK(m.virt == size);
    CHECK(m.table.size() == table_size(m.entries));
    auto r = open(m);
    if (!CHECK(r.has_value())) return;
    Bytes out;
    CHECK(read_all(*r, m, out));
    CHECK(out == img);
    // Ranges: every start near the block edges and at random, odd lengths.
    std::mt19937 rng(seed);
    const auto fetch = [&](uint64_t off, std::span<uint8_t> o) {
        if (off > m.data.size() || o.size() > m.data.size() - off) return false;
        std::memcpy(o.data(), m.data.data() + off, o.size());
        return true;
    };
    for (int i = 0; i < 300; i++) {
        const size_t off = rng() % size, len = std::min<size_t>(size - off, rng() % (3 * bs));
        Bytes        got(len);
        if (!CHECK(r->read(off, got, fetch))) return;
        CHECK(std::equal(got.begin(), got.end(), img.begin() + (long)off));
    }
    { Bytes two(2); CHECK(!r->read(size - 1, two, fetch)); }   // past the end
    // Entries: compressed ones start 16-aligned and are at most a block.
    for (const Entry& e : r->all())
        if (e.kind == Kind::Lz4) {
            CHECK(e.phys % 16 == 0);
            CHECK(e.phys_size > 0 && (uint32_t)e.phys_size <= bs);
        }
}

// With PLAYSHELF_WRITE_SEEDS=<dir>, a small table is written there as the
// fuzzer's seed (tests/fuzz/compressed_storage/corpus): entry count, data
// size, table.
void write_seed()
{
    const char* dir = std::getenv("PLAYSHELF_WRITE_SEEDS");
    if (!dir) return;
    Made m = build(image(120'000, 31), 4096, 4096);
    Bytes seed(8);
    put_le32(seed.data(), m.entries);
    put_le32(seed.data() + 4, (uint32_t)m.data.size());
    seed.insert(seed.end(), m.table.begin(), m.table.end());
    std::ofstream(std::string(dir) + "/table", std::ios::binary).write((const char*)seed.data(), (long)seed.size());
}

void test_roundtrips()
{
    write_seed();
    roundtrip(100'000, 4096, 4096, 1);
    roundtrip(1'000'000, 65536, 777, 2);        // a block size, pieces that do not divide it
    roundtrip(65536 * 3 + 5, 65536, 65536, 3);  // a short last block
    roundtrip(5'000'000, 4096, 100'000, 4);     // 1200+ blocks: several entry sets (stored runs merge, so the count varies)
    roundtrip(4096, 4096, 1, 5);                // one block, fed byte by byte
    roundtrip(1, 4096, 1, 6);                   // one byte
}

void test_tables()
{
    // Zeros compress into one entry per block: many entries, several sets.
    Bytes zeros(4096ull * 1500, 0);
    Made  m = build(zeros, 4096, 1 << 20);
    CHECK(m.ok);
    CHECK(m.entries == 1500);
    CHECK(m.table.size() == (size_t)NODE_SIZE * (1 + 3));   // 1500 entries: 3 sets
    auto r = open(m);
    CHECK(r.has_value());
    Bytes out;
    if (r) { CHECK(read_all(*r, m, out)); CHECK(out == zeros); }

    // Random bytes never shrink: one stored entry for the whole image.
    std::mt19937 rng(9);
    Bytes        noise(300'000);
    for (auto& b : noise) b = (uint8_t)rng();
    m = build(noise, 16384, 5000);
    CHECK(m.ok);
    CHECK(m.entries == 1);
    CHECK(m.data.size() == noise.size() + (16 - noise.size() % 16) % 16);
    auto r2 = open(m);
    CHECK(r2.has_value());
    if (r2) { CHECK(read_all(*r2, m, out)); CHECK(out == noise); }

    // The FS header's copy.
    auto h = table_header(1500);
    CHECK(std::memcmp(h.data(), "BKTR", 4) == 0 && le32(h.data() + 4) == 1 && le32(h.data() + 8) == 1500);
}

void test_builder_refusals()
{
    CHECK(!build({}, 4096, 1).ok);                                        // empty
    Made m;
    Builder bad(1000, lz4_compress, lz4_decompress, [](std::span<const uint8_t>) { return true; });
    CHECK(!bad.add(Bytes(10)));
    Builder small(4096, lz4_compress, lz4_decompress, [](std::span<const uint8_t>) { return true; });
    CHECK(small.add(Bytes(10)) && small.finish());
    CHECK(!small.add(Bytes(10)));                                          // after the end
    // A sink that stops.
    Builder stop(4096, lz4_compress, lz4_decompress, [](std::span<const uint8_t>) { return false; });
    CHECK(!(stop.add(Bytes(8192, 7)) && stop.finish()));
    // A codec that lies about what it made.
    const Bytes img = image(200'000, 11);
    auto        liar = [](std::span<const uint8_t>, std::span<uint8_t> out) -> bool {
        std::memset(out.data(), 0x55, out.size());
        return true;
    };
    CHECK(!build(img, 4096, 4096, lz4_compress, liar).ok);
    // A codec that cannot shrink anything: everything stored, still valid.
    Made s = build(img, 4096, 4096, [](std::span<const uint8_t>, std::span<uint8_t>) { return std::nullopt; });
    CHECK(s.ok && s.entries == 1);
}

// One byte of the table changed (or the table cut), and whether it is refused.
void test_reader_refusals()
{
    // Blocks alternately random and text: one entry each, two entry sets.
    std::mt19937 rng(21);
    Bytes        img(4096 * 1000);
    for (size_t i = 0; i < img.size(); i++)
        img[i] = (i / 4096) % 2 ? (uint8_t)rng() : (uint8_t)("abcdefgh"[(i / 3) % 8]);
    Made m = build(img, 4096, 8192);
    if (!CHECK(m.ok && m.entries == 1000)) return;
    CHECK(open(m).has_value());
    const uint32_t n = m.entries;
    const size_t   set1 = NODE_SIZE * 2;           // the second entry set
    auto           refuses = [&](const char* what, auto&& edit) {
        Made t = m;
        edit(t);
        const bool ok = !open(t).has_value();
        if (!ok) fprintf(stderr, "not refused: %s\n", what);
        CHECK(ok);
    };
    refuses("short table", [&](Made& t) { t.table.resize(t.table.size() - 1); });
    refuses("zero entries", [&](Made& t) { t.entries = 0; });
    refuses("too many entries", [&](Made& t) { t.entries = MAX_ENTRIES + 1; });
    refuses("l1 index", [&](Made& t) { t.table[0] = 1; });
    refuses("l1 count", [&](Made& t) { t.table[4]++; });
    refuses("l1 end 0", [&](Made& t) { put_le64(&t.table[8], 0); });
    refuses("l1 end moved", [&](Made& t) { put_le64(&t.table[8], t.virt + 1); });
    refuses("l1 offset", [&](Made& t) { t.table[NODE_HEADER + 8]++; });
    refuses("set index", [&](Made& t) { t.table[set1] = 7; });
    refuses("set count", [&](Made& t) { t.table[set1 + 4]--; });
    refuses("set end", [&](Made& t) { t.table[set1 + 8]++; });
    refuses("first entry not 0", [&](Made& t) { t.table[NODE_SIZE + NODE_HEADER] = 1; });
    refuses("entry order", [&](Made& t) {
        uint8_t* e = &t.table[NODE_SIZE + NODE_HEADER + ENTRY_SIZE * 3];
        put_le64(e, le64(e) + (1ull << 40));
    });
    refuses("phys past data", [&](Made& t) { put_le64(&t.table[NODE_SIZE + NODE_HEADER + 8], t.data.size() + 1); });
    refuses("kind", [&](Made& t) {
        for (uint32_t i = 0; i < ENTRIES_PER_SET; i++) {
            uint8_t* e = &t.table[NODE_SIZE + NODE_HEADER + ENTRY_SIZE * i];
            if (e[0x10] == 3) { e[0x10] = 2; return; }
        }
    });
    refuses("lz4 size 0", [&](Made& t) {
        for (uint32_t i = 0; i < ENTRIES_PER_SET; i++) {
            uint8_t* e = &t.table[NODE_SIZE + NODE_HEADER + ENTRY_SIZE * i];
            if (e[0x10] == 3) { put_le32(e + 0x14, 0); return; }
        }
    });
    refuses("lz4 size past data", [&](Made& t) {
        for (uint32_t i = 0; i < ENTRIES_PER_SET; i++) {
            uint8_t* e = &t.table[NODE_SIZE + NODE_HEADER + ENTRY_SIZE * i];
            if (e[0x10] == 3) { put_le32(e + 0x14, 0xffff); put_le64(e + 8, t.data.size() - 16); return; }
        }
    });
    refuses("lz4 misaligned", [&](Made& t) {
        for (uint32_t i = 0; i < ENTRIES_PER_SET; i++) {
            uint8_t* e = &t.table[NODE_SIZE + NODE_HEADER + ENTRY_SIZE * i];
            if (e[0x10] == 3) { put_le64(e + 8, le64(e + 8) + 1); return; }
        }
    });
    refuses("data shorter than the entries", [&](Made& t) { t.data.resize(t.data.size() / 2); });
    (void)n;

    // A block that does not decompress to its size: the read fails, nothing is returned as good.
    auto r = open(m);
    if (CHECK(r.has_value())) {
        Made t = m;
        for (auto& b : t.data) b ^= 0xa5;
        auto rt = open(t);
        Bytes out;
        if (CHECK(rt.has_value())) CHECK(!read_all(*rt, t, out));
    }
}
}   // namespace

int main()
{
    test_roundtrips();
    test_tables();
    test_builder_refusals();
    test_reader_refusals();
    CHECK_DONE("compressed_storage: ok");
}
