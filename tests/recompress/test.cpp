// install/recompress.cpp: a Program NCA built here (an ExeFS section and a
// RomFS section with its integrity tree, hash levels first as the system's
// own are, both in CTR) made over with its RomFS compressed, then read back
// by code that does not share the builder's logic: the header parses and
// its hashes match, the first section is the same bytes decrypted, the
// tree verifies, the compression table opens and the RomFS comes out whole,
// byte for byte. And what is refused before a byte is made, and what stops
// it half way (a sink that fails, a cancel, a read that fails).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <lz4.h>
#include <lz4hc.h>

#include <atomic>
#include <cstring>
#include <random>

#include "../install/builders.hpp"
#include "check.h"
#include "install/aes.hpp"
#include "install/ivfc.hpp"
#include "install/recompress.hpp"

using namespace install;
using namespace test_install;

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

Bytes romfs_image(size_t n, uint32_t seed)
{
    std::mt19937 rng(seed);
    Bytes        b(n);
    for (size_t i = 0; i < n;) {
        const size_t run = std::min<size_t>(n - i, 500 + rng() % 30000);
        const int    kind = (int)(rng() % 3);
        for (size_t k = 0; k < run; k++)
            b[i + k] = kind == 0 ? (uint8_t)("romfs data "[k % 11]) : kind == 1 ? (uint8_t)rng() : 0;
        i += run;
    }
    return b;
}

struct Built
{
    Bytes nca, exefs_plain, image;
};

// An NCA with the sections at 0x4000: the ExeFS (a PFS0, CTR), the RomFS
// (hash levels, then the data, CTR). `rights`: a rights id; `type`: content type.
Built program_nca(size_t image_size, uint32_t seed, bool rights = false, uint8_t type = 0)
{
    Built b;
    b.image = romfs_image(image_size, seed);
    b.exefs_plain = archive(PfsKind::Pfs0, {{"main", stream(5000, "main")}, {"main.npdm", stream(900, "npdm")}});

    // Section 0: the PFS0 under a SHA-256 table.
    const size_t   hash_table = 0x20;
    Bytes          s0(((hash_table + b.exefs_plain.size() + 0x1ff) / 0x200) * 0x200);
    std::memcpy(s0.data() + hash_table, b.exefs_plain.data(), b.exefs_plain.size());

    // Section 1: hash levels, then the data.
    ivfc::Builder tree;
    tree.add(b.image);
    auto         r = tree.finish();
    const size_t data_pad = (0x4000 - b.image.size() % 0x4000) % 0x4000;
    Bytes        s1 = r.hash_levels;
    const uint64_t prefix = r.hash_levels.size();
    s1.insert(s1.end(), b.image.begin(), b.image.end());
    s1.insert(s1.end(), data_pad, 0);
    ivfc::Layout l = r.layout;
    for (size_t i = 0; i < 5; i++)
        l.offset[i] -= b.image.size();   // the levels now come first
    l.offset[5] = prefix;
    s1.resize(((s1.size() + 0x1ff) / 0x200) * 0x200);

    const uint64_t s0_start = NCA_FULL_HEADER, s1_start = s0_start + s0.size();
    b.nca.assign(NCA_FULL_HEADER + s0.size() + s1.size(), 0);
    uint8_t* h = b.nca.data();
    put_le32(h + 0x200, 0x3341434e);
    h[0x204] = 1;   // game card: becomes a download NCA
    h[0x205] = type;
    h[0x206] = 2;
    h[0x220] = 3;
    put_le64(h + 0x208, b.nca.size());
    put_le64(h + 0x210, 0x0100000000020000ull);
    if (rights) h[0x230] = 0x77;

    uint8_t fs0[0x200] = {}, fs1[0x200] = {};
    put_le16(fs0, 2); fs0[2] = 1; fs0[3] = 2; fs0[4] = 3;
    put_le64(fs0 + 0x40, hash_table);
    put_le64(fs0 + 0x48, b.exefs_plain.size());
    put_le64(fs0 + 0x140, 0x0000000500000000ull);
    put_le16(fs1, 2); fs1[2] = 0; fs1[3] = 3; fs1[4] = 3;
    ivfc::write_fs_header(fs1, l);
    put_le64(fs1 + 0x140, 0x0000000600000000ull);
    put_le32(h + 0x240, (uint32_t)(s0_start / 0x200));
    put_le32(h + 0x244, (uint32_t)(s1_start / 0x200));
    put_le32(h + 0x250, (uint32_t)(s1_start / 0x200));
    put_le32(h + 0x254, (uint32_t)(b.nca.size() / 0x200));
    std::memcpy(h + 0x400, fs0, 0x200);
    std::memcpy(h + 0x600, fs1, 0x200);
    for (int i = 0; i < 2; i++) {
        Sha256 sh;
        sh.update({h + 0x400 + 0x200 * i, 0x200});
        const auto d = sh.digest();
        std::memcpy(h + 0x280 + 0x20 * i, d.data(), 32);
    }
    uint8_t key_area[0x40] = {};
    std::memcpy(key_area + 0x20, SECTION_KEY, 16);
    aes128_ecb_encrypt(KAEK, key_area);
    std::memcpy(h + 0x300, key_area, 0x40);
    for (size_t i = NCA_HEADER; i < NCA_FULL_HEADER; i++)
        h[i] = (uint8_t)(0xc0 + i % 7);   // padding with something to carry over

    std::memcpy(b.nca.data() + s0_start, s0.data(), s0.size());
    std::memcpy(b.nca.data() + s1_start, s1.data(), s1.size());
    NcaSection n0, n1;
    n0.ctr = le64(fs0 + 0x140);
    n1.ctr = le64(fs1 + 0x140);
    const auto iv0 = nca_section_iv(n0), iv1 = nca_section_iv(n1);
    Aes128Ctr(SECTION_KEY, iv0.data()).crypt(std::span(b.nca).subspan(s0_start, s0.size()), s0_start);
    Aes128Ctr(SECTION_KEY, iv1.data()).crypt(std::span(b.nca).subspan(s1_start, s1.size()), s1_start);
    nca_header_crypt(h, HEADER_KEY, true);
    return b;
}

struct Output
{
    Bytes    nca;
    uint64_t before = 0, after = 0;
};

ProgramRecompressor make(const Bytes& nca, uint32_t block = 65536, NcaKeys keys = test_keys())
{
    return ProgramRecompressor(
        [&nca](uint64_t off, std::span<uint8_t> out) {
            if (off > nca.size() || out.size() > nca.size() - off) return false;
            std::memcpy(out.data(), nca.data() + off, out.size());
            return true;
        },
        nca.size(), std::move(keys), lz4_compress, lz4_decompress, block);
}

bool run(ProgramRecompressor& rc, Output& out, std::string* why = nullptr)
{
    Bytes body;
    if (auto d = rc.inspect(); !d) {
        if (why) *why = d.error();
        return false;
    }
    uint64_t last_done = 0, last_total = 0;
    auto     d = rc.build(
        [&](std::span<const uint8_t> b) {
            body.insert(body.end(), b.begin(), b.end());
            return true;
        },
        [&](uint64_t done, uint64_t total) {
            CHECK(done >= last_done && done <= total);
            last_done = done;
            last_total = total;
        });
    if (!d) {
        if (why) *why = d.error();
        return false;
    }
    CHECK(last_done == last_total && last_total > 0);
    out.nca = rc.new_header();
    out.nca.insert(out.nca.end(), body.begin(), body.end());
    out.before = rc.romfs_before();
    out.after = rc.romfs_after();
    return true;
}

// The section's bytes decrypted, from an NCA made here.
Bytes plain_section(const Bytes& nca, const NcaHeader& h, int i)
{
    const NcaSection& s = h.sections[i];
    Bytes             b(nca.begin() + (long)s.start, nca.begin() + (long)s.end);
    const auto        iv = nca_section_iv(s);
    Aes128Ctr(SECTION_KEY, iv.data()).crypt(b, s.start);
    return b;
}

void check_output(const Built& in, const Output& out)
{
    CHECK(out.nca.size() % 0x200 == 0);
    uint8_t head[NCA_HEADER];
    std::memcpy(head, out.nca.data(), NCA_HEADER);
    nca_header_crypt(head, HEADER_KEY, false);
    auto h = nca_parse_header(head, out.nca.size());   // size and every fs header hash agree
    if (!CHECK(h.has_value())) return;
    CHECK(h->distribution == 0 && h->content_type == 0 && h->program_id == 0x0100000000020000ull);
    CHECK(h->compressed());
    CHECK(h->sections[0].present && h->sections[1].present && !h->sections[2].present);
    CHECK(h->sections[0].start == NCA_FULL_HEADER && h->sections[0].end == h->sections[1].start);
    CHECK(h->sections[1].end == out.nca.size());
    CHECK(std::memcmp(out.nca.data() + NCA_HEADER, in.nca.data() + NCA_HEADER, NCA_FULL_HEADER - NCA_HEADER) == 0);

    // The ExeFS: the same bytes.
    const Bytes ex = plain_section(out.nca, *h, 0);
    CHECK(ex.size() >= 0x20 + in.exefs_plain.size() && std::memcmp(ex.data() + 0x20, in.exefs_plain.data(), in.exefs_plain.size()) == 0);

    // The RomFS: tree, table, image.
    const Bytes       s1 = plain_section(out.nca, *h, 1);
    const NcaSection& sec = h->sections[1];
    auto              l = ivfc::read_fs_header(sec.fs_header.data());
    if (!CHECK(l.has_value())) return;
    CHECK(l->offset[5] == 0);
    CHECK(ivfc::verify(*l, [&](uint64_t off, std::span<uint8_t> o) {
        if (off > s1.size() || o.size() > s1.size() - off) return false;
        std::memcpy(o.data(), s1.data() + off, o.size());
        return true;
    }).has_value());
    const uint8_t* fs = sec.fs_header.data();
    const uint64_t table_offset = le64(fs + 0x178), table_size = le64(fs + 0x180);
    CHECK(table_offset % 0x4000 == 0 && table_size % 0x4000 == 0 && table_offset + table_size == l->size[5]);
    CHECK(le32(fs + 0x188) == 0x52544b42 && le32(fs + 0x18c) == 1);
    CHECK(sec.compressed());
    auto reader = cstore::Reader::open({s1.data() + table_offset, (size_t)table_size}, (int32_t)le32(fs + 0x190), table_offset, lz4_decompress);
    if (!CHECK(reader.has_value())) return;
    CHECK(reader->virtual_size() == in.image.size());
    Bytes image(in.image.size());
    CHECK(reader->read(0, image, [&](uint64_t off, std::span<uint8_t> o) {
        if (off > table_offset || o.size() > table_offset - off) return false;
        std::memcpy(o.data(), s1.data() + off, o.size());
        return true;
    }));
    CHECK(image == in.image);
}

void test_roundtrips()
{
    for (size_t n : {size_t(1), size_t(5000), size_t(0x4000), size_t(70'000), size_t(1'200'000)}) {
        for (uint32_t block : {4096u, 65536u}) {
            const Built in = program_nca(n, (uint32_t)n, false);
            auto        rc = make(in.nca, block);
            Output      out;
            std::string why;
            if (!CHECK(run(rc, out, &why))) {
                fprintf(stderr, "size %zu block %u: %s\n", n, block, why.c_str());
                continue;
            }
            check_output(in, out);
            if (n >= 1'000'000) CHECK(out.after < out.before);   // text and zeros shrink
        }
    }
}

ReadAtFn reader_of(const Bytes& b)
{
    return [&b](uint64_t off, std::span<uint8_t> out) {
        if (off > b.size() || out.size() > b.size() - off) return false;
        std::memcpy(out.data(), b.data() + off, out.size());
        return true;
    };
}

void test_verify()
{
    const Built in = program_nca(900'000, 8);
    auto        rc = make(in.nca, 4096);
    Output      out;
    CHECK(run(rc, out));
    uint64_t last = 0, total = 0;
    CHECK(rc.verify(reader_of(out.nca), out.nca.size(), [&](uint64_t d, uint64_t t) { CHECK(d >= last && d <= t); last = d; total = t; }).has_value());
    CHECK(last == total && total > 0);

    // The NCA's layout, to aim at its parts.
    uint8_t head[NCA_HEADER];
    std::memcpy(head, out.nca.data(), NCA_HEADER);
    nca_header_crypt(head, HEADER_KEY, false);
    const auto h = nca_parse_header(head, out.nca.size());
    if (!CHECK(h.has_value())) return;
    const uint64_t s0 = h->sections[0].start, s1 = h->sections[1].start;
    const uint8_t* fs = h->sections[1].fs_header.data();
    const uint64_t table_offset = le64(fs + 0x178);

    auto flips = [&](const char* what, uint64_t at) {
        Bytes bad = out.nca;
        bad[at] ^= 0x10;
        auto       rc2 = make(in.nca, 4096);
        CHECK(rc2.inspect().has_value());
        const bool refused = !rc2.verify(reader_of(bad), bad.size()).has_value();
        if (!refused) fprintf(stderr, "not found: %s\n", what);
        CHECK(refused);
    };
    flips("a byte of the header", 0x300);
    flips("a byte of the fs header", 0x400 + 0x200 + 0x10);
    flips("a byte of the ExeFS", s0 + 0x100);
    flips("a byte of the compressed data", s1 + 100);
    flips("a byte of the compressed data, late", s1 + table_offset - 40);
    flips("a byte of the table", s1 + table_offset + 0x4000 + 0x20);
    flips("a byte of the hash levels", s1 + table_offset + le64(fs + 0x180) + 5);
    {   // cut short, or longer
        Bytes cut(out.nca.begin(), out.nca.end() - 0x200);
        auto  rc2 = make(in.nca, 4096);
        CHECK(rc2.inspect().has_value());
        CHECK(!rc2.verify(reader_of(cut), cut.size()).has_value());
        CHECK(!rc2.verify(reader_of(out.nca), 0x100).has_value());
    }
    {   // another NCA altogether: the same program, a different RomFS
        const Built other = program_nca(900'000, 9);
        auto        rc3 = make(other.nca, 4096);
        Output      out3;
        CHECK(run(rc3, out3));
        auto rc2 = make(in.nca, 4096);
        CHECK(rc2.inspect().has_value());
        CHECK(!rc2.verify(reader_of(out3.nca), out3.nca.size()).has_value());
    }
    {   // a cancel
        std::atomic<bool> stop{true};
        auto              rc2 = make(in.nca, 4096);
        CHECK(rc2.inspect().has_value());
        CHECK(!rc2.verify(reader_of(out.nca), out.nca.size(), {}, &stop).has_value());
    }
    {   // before inspect()
        auto rc2 = make(in.nca, 4096);
        CHECK(!rc2.verify(reader_of(out.nca), out.nca.size()).has_value());
    }
}

void test_inspect_refusals()
{
    auto refused = [](const char* what, const Bytes& nca, uint32_t block = 65536, NcaKeys keys = test_keys()) {
        auto        rc = make(nca, block, std::move(keys));
        const bool  r = !rc.inspect().has_value();
        if (!r) fprintf(stderr, "not refused: %s\n", what);
        CHECK(r);
    };
    refused("rights id", program_nca(70'000, 1, true).nca);
    refused("not a program", program_nca(70'000, 1, false, 2).nca);
    refused("block size", program_nca(70'000, 1).nca, 5000);
    NcaKeys wrong = test_keys();
    wrong.header_key[0] ^= 1;
    refused("header key", program_nca(70'000, 1).nca, 65536, wrong);
    NcaKeys nokey = test_keys();
    nokey.unwrap = [](uint8_t, uint8_t, const uint8_t*, uint8_t*) { return false; };
    refused("no section key", program_nca(70'000, 1).nca, 65536, nokey);
    refused("too small", Bytes(0x3000));
    {   // already compressed: the output of a run
        const Built in = program_nca(300'000, 2);
        auto        rc = make(in.nca);
        Output      out;
        CHECK(run(rc, out));
        refused("already compressed", out.nca);
    }
}

void test_stops()
{
    const Built in = program_nca(900'000, 4);
    {   // a sink that fails, early and late
        for (size_t fail_at : {size_t(0), size_t(3), size_t(50)}) {
            auto   rc = make(in.nca, 4096);
            CHECK(rc.inspect().has_value());
            size_t calls = 0;
            CHECK(!rc.build([&](std::span<const uint8_t>) { return calls++ != fail_at; }).has_value());
        }
    }
    {   // a cancel
        std::atomic<bool> stop{false};
        auto              rc = make(in.nca, 4096);
        CHECK(rc.inspect().has_value());
        CHECK(!rc.build([&](std::span<const uint8_t>) { stop = true; return true; }, {}, &stop).has_value());
    }
    {   // a read that fails after the header
        size_t reads = 0;
        ProgramRecompressor rc(
            [&](uint64_t off, std::span<uint8_t> out) {
                if (++reads > 2) return false;
                std::memcpy(out.data(), in.nca.data() + off, out.size());
                return true;
            },
            in.nca.size(), test_keys(), lz4_compress, lz4_decompress, 4096);
        CHECK(rc.inspect().has_value());
        CHECK(!rc.build([](std::span<const uint8_t>) { return true; }).has_value());
    }
    {   // build() before inspect()
        auto rc = make(in.nca);
        CHECK(!rc.build([](std::span<const uint8_t>) { return true; }).has_value());
    }
    {   // a codec that lies stops the build, nothing is claimed
        ProgramRecompressor rc(
            [&](uint64_t off, std::span<uint8_t> out) {
                std::memcpy(out.data(), in.nca.data() + off, out.size());
                return true;
            },
            in.nca.size(), test_keys(), lz4_compress,
            [](std::span<const uint8_t>, std::span<uint8_t> o) { std::memset(o.data(), 1, o.size()); return true; }, 4096);
        CHECK(rc.inspect().has_value());
        CHECK(!rc.build([](std::span<const uint8_t>) { return true; }).has_value());
    }
}
}   // namespace

int main()
{
    test_roundtrips();
    test_verify();
    test_inspect_refusals();
    test_stops();
    CHECK_DONE("recompress: ok");
}
