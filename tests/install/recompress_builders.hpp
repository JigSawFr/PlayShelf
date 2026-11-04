// Inputs the recompress suites build: a Program NCA (an ExeFS and a RomFS
// with its integrity tree, in CTR), the contents of a title and the
// .cnmt.nca that holds its content meta, and an LZ4 codec from liblz4.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <lz4.h>
#include <lz4hc.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <random>

#include "builders.hpp"
#include "install/aes.hpp"
#include "install/ivfc.hpp"

namespace test_install
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

std::vector<ContentInfo> contents()
{
    ContentInfo program;
    program.id = id_of(0x11);
    program.size = 0x1234567;
    program.type = ContentType::Program;
    program.hash.fill(0xaa);
    ContentInfo control;
    control.id = id_of(0x22);
    control.size = 0x40000;
    control.type = ContentType::Control;
    control.hash.fill(0xbb);
    return {program, control};
}

ContentInfo now()
{
    ContentInfo c;
    c.hash.fill(0x5c);
    c.id = id_of(0x99);
    c.size = 0x2'0000'1000ull;   // beyond 32 bits
    c.type = ContentType::Program;
    return c;
}

// A .cnmt.nca with a real partition hash table: block size 0x1000, the table
// first, the PFS0 at the next 0x200.
Bytes meta_nca(const Bytes& cnmt, uint32_t block = 0x1000, uint8_t distribution = 0)
{
    const Bytes    pfs = archive(PfsKind::Pfs0, {{"Application_0100000000010000.cnmt", cnmt}});
    const uint64_t table_size = (pfs.size() + block - 1) / block * 32;
    const uint64_t pfs_at = (table_size + 0x1ff) / 0x200 * 0x200;
    const size_t   section = (pfs_at + pfs.size() + 0x1ff) / 0x200 * 0x200;
    Bytes          nca(NCA_FULL_HEADER + section);
    uint8_t*       h = nca.data();
    put_le32(h + 0x200, 0x3341434e);
    h[0x204] = distribution;
    h[0x205] = 1;
    h[0x206] = 2;
    h[0x220] = 3;
    put_le64(h + 0x208, nca.size());
    put_le64(h + 0x210, 0x0100000000010000ull);
    put_le32(h + 0x240, NCA_FULL_HEADER / 0x200);
    put_le32(h + 0x244, (uint32_t)(nca.size() / 0x200));
    uint8_t* fs = h + 0x400;
    put_le16(fs, 2);
    fs[2] = 1;
    fs[3] = 2;
    fs[4] = 3;
    put_le32(fs + 0x28, block);
    put_le32(fs + 0x2c, 2);
    put_le64(fs + 0x30, 0);
    put_le64(fs + 0x38, table_size);
    put_le64(fs + 0x40, pfs_at);
    put_le64(fs + 0x48, pfs.size());
    put_le64(fs + 0x140, 0x0000000500000000ull);

    uint8_t* body = nca.data() + NCA_FULL_HEADER;
    std::memcpy(body + pfs_at, pfs.data(), pfs.size());
    for (uint64_t i = 0; i < table_size / 32; i++) {
        Sha256 sh;
        sh.update({pfs.data() + i * block, std::min<size_t>(block, pfs.size() - i * block)});
        const auto d = sh.digest();
        std::memcpy(body + i * 32, d.data(), 32);
    }
    Sha256 m;
    m.update({body, (size_t)table_size});
    const auto md = m.digest();
    std::memcpy(fs + 0x08, md.data(), 32);
    Sha256 f;
    f.update({fs, 0x200});
    const auto fd = f.digest();
    std::memcpy(h + 0x280, fd.data(), 32);
    uint8_t key_area[0x40] = {};
    std::memcpy(key_area + 0x20, SECTION_KEY, 16);
    aes128_ecb_encrypt(KAEK, key_area);
    std::memcpy(h + 0x300, key_area, 0x40);
    NcaSection s;
    s.ctr = le64(fs + 0x140);
    const auto iv = nca_section_iv(s);
    Aes128Ctr(SECTION_KEY, iv.data()).crypt(std::span(nca).subspan(NCA_FULL_HEADER, section), NCA_FULL_HEADER);
    nca_header_crypt(h, HEADER_KEY, true);
    return nca;
}


}   // namespace test_install
