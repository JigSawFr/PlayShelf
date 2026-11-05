// install/content_stream.cpp: an NCA built here, fed as a .nca and as the
// .ncz nsz would make of it (one zstd stream, and NCZBLOCK blocks, some
// compressed, some stored), in pieces of every size: the NCA comes out byte
// for byte, its SHA-256 the published one; a game card one comes out a
// download one. And what is refused: a wrong hash, a size other than the
// content meta's, data cut short or past the end, broken NCZ headers and
// blocks, a header the caller refuses.
// With PLAYSHELF_WRITE_SEEDS=<dir>, the inputs are written there as the
// fuzzer's seeds (tests/fuzz/install_stream/corpus).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <fstream>
#include <zstd.h>

#include "../install/builders.hpp"
#include "check.h"
#include "install/aes.hpp"
#include "install/content_stream.hpp"
#include "install/nca.hpp"

using namespace install;
using namespace test_install;

namespace
{
const uint8_t HEADER_KEY[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
const uint8_t SECTION_KEY[16] = {0x5e, 0xc7, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d};
const uint64_t CTR0 = 0x0000000700000000ull, CTR1 = 0x0000000800000000ull;

void write_seed(const char* name, const Bytes& b)
{
    const char* dir = std::getenv("PLAYSHELF_WRITE_SEEDS");
    if (!dir) return;
    std::ofstream(std::string(dir) + "/" + name, std::ios::binary).write((const char*)b.data(), (long)b.size());
}

std::array<uint8_t, 32> sha(const Bytes& b)
{
    Sha256 h;
    h.update(b);
    return h.digest();
}

std::array<uint8_t, 16> iv_of(uint64_t ctr)
{
    NcaSection s;
    s.ctr = ctr;
    return nca_section_iv(s);
}

struct Built
{
    Bytes nca;     // as published
    Bytes plain;   // its body from 0x4000, the CTR sections decrypted (what nsz compresses)
    uint64_t s0 = 0, s0_size = 0, s1 = 0, s1_size = 0, s2 = 0, s2_size = 0;
};

// An NCA of three sections: two CTR (one compressible, one not), one not
// encrypted.
Built build(uint8_t distribution = 0)
{
    Built b;
    b.s0 = NCA_FULL_HEADER;
    b.s0_size = 0x9000;
    b.s1 = b.s0 + b.s0_size;
    b.s1_size = 0x6200;
    b.s2 = b.s1 + b.s1_size;
    b.s2_size = 0x1e00;
    const uint64_t size = b.s2 + b.s2_size;
    b.plain.resize(size - NCA_FULL_HEADER);
    for (size_t i = 0; i < b.s0_size; i++)
        b.plain[i] = (uint8_t)("compressible content "[i % 21]);
    const Bytes noise = stream(b.s1_size + b.s2_size, "noise");
    std::copy(noise.begin(), noise.end(), b.plain.begin() + (long)b.s0_size);

    b.nca.assign(size, 0);
    uint8_t* h = b.nca.data();
    put_le32(h + 0x200, 0x3341434e);
    h[0x204] = distribution;
    h[0x205] = 0;   // program
    h[0x220] = 3;
    put_le64(h + 0x208, size);
    put_le64(h + 0x210, 0x0100000000010000ull);
    const uint64_t at[3] = {b.s0, b.s1, b.s2}, len[3] = {b.s0_size, b.s1_size, b.s2_size};
    const uint8_t  crypt[3] = {3, 3, 1};
    const uint64_t ctr[3] = {CTR0, CTR1, 0};
    for (int i = 0; i < 3; i++) {
        put_le32(h + 0x240 + 0x10 * i, (uint32_t)(at[i] / 0x200));
        put_le32(h + 0x244 + 0x10 * i, (uint32_t)((at[i] + len[i]) / 0x200));
        uint8_t* fs = h + 0x400 + 0x200 * i;
        put_le16(fs, 2);
        fs[2] = 0;
        fs[3] = 3;
        fs[4] = crypt[i];
        put_le64(fs + 0x140, ctr[i]);
        Sha256 s;
        s.update({fs, 0x200});
        const auto d = s.digest();
        std::memcpy(h + 0x280 + 0x20 * i, d.data(), 32);
    }
    nca_header_crypt(h, HEADER_KEY, true);
    std::copy(b.plain.begin(), b.plain.end(), b.nca.begin() + NCA_FULL_HEADER);
    Aes128Ctr(SECTION_KEY, iv_of(CTR0).data()).crypt(std::span(b.nca).subspan(b.s0, b.s0_size), b.s0);
    Aes128Ctr(SECTION_KEY, iv_of(CTR1).data()).crypt(std::span(b.nca).subspan(b.s1, b.s1_size), b.s1);
    return b;
}

// An NCA as the tools that pack homebrew make it: the first section starts right
// after the 0xC00 bytes of header, no padding to 0x4000. `tiny`: one section of
// 0x400 bytes, the whole NCA under 0x4000 (a .cnmt.nca of such a package).
Built build_packed(uint8_t distribution = 0, bool tiny = false)
{
    Built b;
    b.s0 = NCA_HEADER;
    b.s0_size = tiny ? 0x400 : 0x2e00;
    b.s1 = b.s0 + b.s0_size;
    b.s1_size = tiny ? 0 : 0x6200;
    const uint64_t size = b.s1 + b.s1_size;
    b.plain.resize(size - NCA_HEADER);
    for (size_t i = 0; i < b.plain.size(); i++)
        b.plain[i] = i < b.s0_size ? (uint8_t)("packed content "[i % 15]) : (uint8_t)(i * 7 + (i >> 8));

    b.nca.assign(size, 0);
    uint8_t* h = b.nca.data();
    put_le32(h + 0x200, 0x3341434e);
    h[0x204] = distribution;
    h[0x205] = 0;
    h[0x220] = 3;
    put_le64(h + 0x208, size);
    put_le64(h + 0x210, 0x0100000000010000ull);
    const uint64_t at[2] = {b.s0, b.s1}, len[2] = {b.s0_size, b.s1_size};
    const uint64_t ctr[2] = {CTR0, CTR1};
    for (int i = 0; i < (tiny ? 1 : 2); i++) {
        put_le32(h + 0x240 + 0x10 * i, (uint32_t)(at[i] / 0x200));
        put_le32(h + 0x244 + 0x10 * i, (uint32_t)((at[i] + len[i]) / 0x200));
        uint8_t* fs = h + 0x400 + 0x200 * i;
        put_le16(fs, 2);
        fs[2] = 0;
        fs[3] = 3;
        fs[4] = 3;
        put_le64(fs + 0x140, ctr[i]);
        Sha256 s;
        s.update({fs, 0x200});
        const auto d = s.digest();
        std::memcpy(h + 0x280 + 0x20 * i, d.data(), 32);
    }
    nca_header_crypt(h, HEADER_KEY, true);
    std::copy(b.plain.begin(), b.plain.end(), b.nca.begin() + NCA_HEADER);
    Aes128Ctr(SECTION_KEY, iv_of(CTR0).data()).crypt(std::span(b.nca).subspan(b.s0, b.s0_size), b.s0);
    if (!tiny) Aes128Ctr(SECTION_KEY, iv_of(CTR1).data()).crypt(std::span(b.nca).subspan(b.s1, b.s1_size), b.s1);
    return b;
}

// The NCZSECTN header of `b`'s sections.
Bytes sections(const Built& b)
{
    Bytes s(16 + 3 * 0x40);
    put_le64(s.data(), 0x4e544345535a434eull);
    put_le64(s.data() + 8, 3);
    const uint64_t at[3] = {b.s0, b.s1, b.s2}, len[3] = {b.s0_size, b.s1_size, b.s2_size};
    const uint8_t  crypt[3] = {3, 3, 1};
    const uint64_t ctr[3] = {CTR0, CTR1, 0};
    for (int i = 0; i < 3; i++) {
        uint8_t* e = s.data() + 16 + 0x40 * i;
        put_le64(e, at[i]);
        put_le64(e + 8, len[i]);
        e[16] = crypt[i];
        std::memcpy(e + 0x20, SECTION_KEY, 16);
        const auto iv = iv_of(ctr[i]);
        std::memcpy(e + 0x30, iv.data(), 16);
    }
    return s;
}

Bytes zstd_frame(const uint8_t* p, size_t n, int level = 3)
{
    Bytes out(ZSTD_compressBound(n));
    const size_t r = ZSTD_compress(out.data(), out.size(), p, n, level);
    REQUIRE(!ZSTD_isError(r));
    out.resize(r);
    return out;
}

// A .ncz with one zstd stream for the body.
Bytes ncz_stream(const Built& b)
{
    Bytes out(b.nca.begin(), b.nca.begin() + NCA_FULL_HEADER);
    const Bytes s = sections(b), z = zstd_frame(b.plain.data(), b.plain.size());
    out.insert(out.end(), s.begin(), s.end());
    out.insert(out.end(), z.begin(), z.end());
    return out;
}

// A .ncz in NCZBLOCK blocks of 2^exponent bytes: a block that does not
// shrink is stored.
Bytes ncz_blocks(const Built& b, uint8_t exponent = 14)
{
    Bytes out(b.nca.begin(), b.nca.begin() + NCA_FULL_HEADER);
    const Bytes    s = sections(b);
    const uint64_t bs = 1ull << exponent, total = b.plain.size();
    const uint32_t count = (uint32_t)((total + bs - 1) / bs);
    out.insert(out.end(), s.begin(), s.end());
    Bytes head(0x18);
    put_le64(head.data(), 0x4b434f4c425a434eull);
    head[8] = 2;
    head[9] = 1;
    head[11] = exponent;
    put_le32(head.data() + 12, count);
    put_le64(head.data() + 16, total);
    out.insert(out.end(), head.begin(), head.end());
    std::vector<Bytes> blocks;
    for (uint32_t i = 0; i < count; i++) {
        const size_t from = (size_t)(i * bs), n = (size_t)std::min<uint64_t>(bs, total - from);
        Bytes        z = zstd_frame(b.plain.data() + from, n);
        if (z.size() >= n) z.assign(b.plain.begin() + (long)from, b.plain.begin() + (long)(from + n));
        blocks.push_back(z);
        uint8_t sz[4];
        put_le32(sz, (uint32_t)z.size());
        out.insert(out.end(), sz, sz + 4);
    }
    for (const Bytes& z : blocks)
        out.insert(out.end(), z.begin(), z.end());
    return out;
}

struct Result
{
    bool        ok = false;
    std::string error;
    Bytes       out;
    bool        compressed = false;
    int         headers = 0;
    NcaSignature signature = NcaSignature::Valid;
};

// Feeds `file` in pieces of the sizes `sizes` cycles through.
Result run(const Bytes& file, std::optional<std::array<uint8_t, 32>> hash, uint64_t expected_size,
           std::vector<size_t> sizes = {0}, bool refuse = false)
{
    Result        r;
    ContentStream cs(
        HEADER_KEY, hash, expected_size,
        [&](const NcaHeader& h, NcaSignature sig) -> Done {
            r.signature = sig;
            r.headers++;
            r.out.reserve(h.size);
            if (refuse) return fail("refused");
            return {};
        },
        [&](uint64_t offset, const uint8_t* data, size_t size) {
            if (offset != r.out.size()) return false;
            r.out.insert(r.out.end(), data, data + size);
            return true;
        });
    size_t at = 0, k = 0;
    bool   fed = true;
    while (fed && at < file.size()) {
        size_t n = sizes[k++ % sizes.size()];
        if (!n || n > file.size() - at) n = file.size() - at;
        fed = cs.feed(std::span(file).subspan(at, n));
        at += n;
    }
    r.ok = fed && cs.finish();
    r.error = cs.error();
    r.compressed = cs.compressed();
    return r;
}

void test_plain()
{
    const Built b = build();
    const auto  hash = sha(b.nca);
    write_seed("nca", b.nca);
    for (const auto& sizes : std::vector<std::vector<size_t>>{{0}, {1}, {7, 4093, 1}, {0x4000}, {0x3fff, 9}}) {
        const Result r = run(b.nca, hash, b.nca.size(), sizes);
        CHECK(r.ok);
        CHECK(r.out == b.nca);
        CHECK(!r.compressed);
        CHECK(r.headers == 1);
        CHECK(r.signature == NcaSignature::Invalid);   // not signed with Nintendo's key
    }
    // No hash to check: fine too. The wrong one, the wrong size: refused.
    CHECK(run(b.nca, std::nullopt, 0).ok);
    auto wrong = hash;
    wrong[0] ^= 1;
    const Result bad_hash = run(b.nca, wrong, 0);
    CHECK(!bad_hash.ok && bad_hash.error.find("SHA-256") != std::string::npos);
    CHECK(!run(b.nca, hash, b.nca.size() + 1).ok);
    // Cut short; a byte too many; a header the caller refuses.
    CHECK(!run(Bytes(b.nca.begin(), b.nca.end() - 1), hash, 0).ok);
    CHECK(!run(Bytes(b.nca.begin(), b.nca.begin() + 0x3fff), hash, 0).ok);
    Bytes longer = b.nca;
    longer.push_back(0);
    CHECK(!run(longer, std::nullopt, 0).ok);
    const Result refused = run(b.nca, hash, 0, {0}, true);
    CHECK(!refused.ok && refused.error == "refused" && refused.out.empty());
    // Not an NCA: the header never parses.
    Bytes junk = b.nca;
    junk[0x200] ^= 1;
    CHECK(!run(junk, std::nullopt, 0).ok);
}

void test_game_card()
{
    const Built  b = build(1);
    const Result r = run(b.nca, sha(b.nca), 0);   // the published hash: the game card header's
    if (!CHECK(r.ok)) return;
    CHECK(Bytes(r.out.begin() + NCA_HEADER, r.out.end()) == Bytes(b.nca.begin() + NCA_HEADER, b.nca.end()));
    uint8_t head[NCA_HEADER];
    std::memcpy(head, r.out.data(), NCA_HEADER);
    nca_header_crypt(head, HEADER_KEY, false);
    CHECK(head[0x204] == 0);
}

void test_ncz()
{
    const Built b = build();
    const auto  hash = sha(b.nca);
    const Bytes zs = ncz_stream(b), zb = ncz_blocks(b);
    write_seed("ncz_stream", zs);
    write_seed("ncz_blocks", zb);
    CHECK(zs.size() < b.nca.size() && zb.size() < b.nca.size());
    for (const Bytes* f : {&zs, &zb})
        for (const auto& sizes : std::vector<std::vector<size_t>>{{0}, {1}, {3, 1000, 17}, {0x4000, 8, 0xc0, 4}}) {
            const Result r = run(*f, hash, b.nca.size(), sizes);
            CHECK(r.ok);
            CHECK(r.compressed);
            CHECK(r.out == b.nca);
            if (!r.ok) fprintf(stderr, "  %s\n", r.error.c_str());
        }
    // Blocks of other sizes, the last one short: the same NCA.
    for (uint8_t e : {15, 16, 20}) {
        const Result r = run(ncz_blocks(b, e), hash, 0, {4096});
        CHECK(r.ok && r.out == b.nca);
    }

    // Cut short anywhere after the header: refused.
    for (size_t n = NCA_FULL_HEADER; n < zb.size(); n += 997)
        CHECK(!run(Bytes(zb.begin(), zb.begin() + (long)n), hash, 0).ok);
    for (size_t n = NCA_FULL_HEADER + 1; n < zs.size(); n += 499)
        CHECK(!run(Bytes(zs.begin(), zs.begin() + (long)n), hash, 0).ok);
    // A byte past the end.
    Bytes longer = zb;
    longer.push_back(0);
    CHECK(!run(longer, hash, 0).ok);

    const size_t sec = NCA_FULL_HEADER;
    // Too many sections; a section past the NCA; overlapping sections.
    Bytes bad = zb;
    put_le64(bad.data() + sec + 8, 65);
    CHECK(!run(bad, hash, 0).ok);
    bad = zb;
    put_le64(bad.data() + sec + 16 + 0x40 * 2 + 8, b.s2_size + 1);
    CHECK(!run(bad, hash, 0).ok);
    bad = zb;
    put_le64(bad.data() + sec + 16 + 0x40 + 0, b.s1 - 0x10);
    CHECK(!run(bad, hash, 0).ok);
    // A wrong key: the bytes come out other, and the hash says so.
    bad = zb;
    bad[sec + 16 + 0x20] ^= 1;
    const Result wrong_key = run(bad, hash, 0);
    CHECK(!wrong_key.ok && wrong_key.error.find("SHA-256") != std::string::npos);

    const size_t blk = sec + 16 + 3 * 0x40;
    // A block size out of range; counts that do not add up; an unknown type.
    bad = zb;
    bad[blk + 11] = 13;
    CHECK(!run(bad, hash, 0).ok);
    bad = zb;
    put_le32(bad.data() + blk + 12, le32(zb.data() + blk + 12) + 1);
    CHECK(!run(bad, hash, 0).ok);
    bad = zb;
    put_le64(bad.data() + blk + 16, b.plain.size() - 1);
    CHECK(!run(bad, hash, 0).ok);
    bad = zb;
    bad[blk + 9] = 2;
    CHECK(!run(bad, hash, 0).ok);
    // A block's size zero, or past what it decompresses to.
    bad = zb;
    put_le32(bad.data() + blk + 0x18, 0);
    CHECK(!run(bad, hash, 0).ok);
    bad = zb;
    put_le32(bad.data() + blk + 0x18, 0x4001);
    CHECK(!run(bad, hash, 0).ok);
    // A body neither zstd nor NCZBLOCK; a broken zstd frame.
    bad = zb;
    bad[blk] ^= 1;
    CHECK(!run(bad, hash, 0).ok);
    bad = zs;
    bad[blk + 20] ^= 0xff;
    CHECK(!run(bad, hash, 0).ok);
}
// A .nca stopped after `cut` bytes of the file, then taken up again by a
// new stream from its checkpoint: the NCA comes out whole, its hash right.
void test_resume()
{
    // The hash's state, saved and taken back halfway through a block.
    {
        const Bytes data = stream(1000, "sha");
        Sha256      whole;
        whole.update(data);
        Sha256 first;
        first.update(std::span(data).first(333));
        const auto state = first.save();
        Sha256     second;
        CHECK(second.restore(state));
        second.update(std::span(data).subspan(333));
        CHECK(second.digest() == whole.digest());
        auto broken = state;
        broken[Sha256::STATE_SIZE - 1] ^= 1;   // a total that no longer matches what is held
        CHECK(!Sha256().restore(broken));
        CHECK(!Sha256().restore(std::span(state).first(state.size() - 1)));
    }

    for (const uint8_t distribution : {0, 1}) {
        const Built b = build(distribution);
        const auto  hash = sha(b.nca);
        const Result whole = run(b.nca, hash, b.nca.size());
        if (!CHECK(whole.ok)) return;
        for (const size_t cut : {(size_t)NCA_FULL_HEADER + 8, (size_t)0x7123, b.nca.size() - 1, b.nca.size()}) {
            Bytes out(b.nca.size());
            auto  write = [&](uint64_t offset, const uint8_t* data, size_t size) {
                std::memcpy(out.data() + offset, data, size);
                return true;
            };
            ContentStream first(HEADER_KEY, hash, b.nca.size(), nullptr, write);
            CHECK(!first.checkpoint());   // nothing of the body yet
            CHECK(first.feed(std::span(b.nca).first(cut)));
            const auto at = first.checkpoint();
            if (!CHECK(at && at->offset == cut && at->nca_size == b.nca.size())) continue;

            ContentStream again(HEADER_KEY, hash, b.nca.size(), nullptr, write);
            CHECK(again.resume_at(*at));
            CHECK(again.feed(std::span(b.nca).subspan(cut)));
            CHECK(again.finish());
            CHECK(out == whole.out);
        }
        // What cannot be this content's: refused, the stream left as it was.
        ContentStream first(HEADER_KEY, hash, b.nca.size(), nullptr, [](uint64_t, const uint8_t*, size_t) { return true; });
        CHECK(first.feed(std::span(b.nca).first(0x6000)));
        const auto at = first.checkpoint();
        if (!CHECK(at)) return;
        auto check = [&](ContentStream::Checkpoint c, uint64_t size) {
            ContentStream s(HEADER_KEY, hash, size, nullptr, [](uint64_t, const uint8_t*, size_t) { return true; });
            return s.resume_at(c);
        };
        CHECK(check(*at, b.nca.size()));
        CHECK(check(*at, 0));   // no size to check against
        CHECK(!check(*at, b.nca.size() + 0x200));
        auto past = *at;
        past.offset = past.nca_size + 1;
        CHECK(!check(past, 0));
        auto early = *at;
        early.offset = 0x200;
        CHECK(!check(early, 0));
        auto other = *at;
        other.sha[0] ^= 1;   // a state, but not this content's: its hash fails at the end
        ContentStream wrong(HEADER_KEY, hash, b.nca.size(), nullptr, [](uint64_t, const uint8_t*, size_t) { return true; });
        CHECK(wrong.resume_at(other));
        CHECK(wrong.feed(std::span(b.nca).subspan(at->offset)));
        CHECK(!wrong.finish() && wrong.error().find("SHA-256") != std::string::npos);
    }

    // A .ncz has no checkpoint: it starts again from its beginning.
    const Built b = build();
    Bytes       ncz(b.nca.begin(), b.nca.begin() + NCA_FULL_HEADER);
    const Bytes head = sections(b);
    ncz.insert(ncz.end(), head.begin(), head.end());
    ContentStream z(HEADER_KEY, std::nullopt, 0, nullptr, [](uint64_t, const uint8_t*, size_t) { return true; });
    CHECK(z.feed(ncz));
    CHECK(!z.checkpoint());
}

// The NCAs packed for homebrew put the first section right after the header: they
// come through as they are, whole or in pieces of every size, however small they
// are, a game card one made a download one, and one can be taken up again.
void test_packed_layout()
{
    for (const bool tiny : {false, true}) {
        const Built b = build_packed(0, tiny);
        const auto  hash = sha(b.nca);
        CHECK(tiny == (b.nca.size() < NCA_FULL_HEADER));
        for (const auto& sizes : std::vector<std::vector<size_t>>{{0}, {1}, {7, 4093, 1}, {0xc00}, {0xbff, 9}}) {
            const Result r = run(b.nca, hash, b.nca.size(), sizes);
            if (!CHECK(r.ok)) fprintf(stderr, "packed (tiny %d): %s\n", (int)tiny, r.error.c_str());
            CHECK(r.out == b.nca);
            CHECK(!r.compressed && r.headers == 1);
        }
        // Cut short anywhere in the header region, or the body; a byte too many; a wrong hash.
        for (const size_t cut : {(size_t)0x10, (size_t)NCA_HEADER - 1, (size_t)NCA_HEADER, (size_t)NCA_HEADER + 1, b.nca.size() - 1})
            CHECK(!run(Bytes(b.nca.begin(), b.nca.begin() + (long)cut), hash, 0).ok);
        Bytes longer = b.nca;
        longer.push_back(0);
        CHECK(!run(longer, std::nullopt, 0).ok);
        auto wrong = hash;
        wrong[3] ^= 1;
        CHECK(!run(b.nca, wrong, 0).ok);

        // A game card one is made a download one, and the hash is the published one's.
        const Built  card = build_packed(1, tiny);
        const Result r = run(card.nca, sha(card.nca), 0);
        if (CHECK(r.ok)) {
            uint8_t head[NCA_HEADER];
            std::memcpy(head, r.out.data(), NCA_HEADER);
            nca_header_crypt(head, HEADER_KEY, false);
            CHECK(head[0x204] == 0);
            CHECK(std::equal(r.out.begin() + NCA_HEADER, r.out.end(), card.nca.begin() + NCA_HEADER));
        }

        // Taken up again from inside the body (the header region is behind it).
        const Result whole = run(b.nca, hash, b.nca.size());
        for (const size_t cut : {(size_t)NCA_HEADER, (size_t)NCA_HEADER + 3, b.nca.size() - 1, b.nca.size()}) {
            if (cut > b.nca.size()) continue;
            Bytes out(b.nca.size());
            auto  write = [&](uint64_t offset, const uint8_t* data, size_t size) {
                std::memcpy(out.data() + offset, data, size);
                return true;
            };
            ContentStream first(HEADER_KEY, hash, b.nca.size(), nullptr, write);
            CHECK(first.feed(std::span(b.nca).first(cut)));
            const auto at = first.checkpoint();
            if (!at) continue;   // right after the header: no body byte yet
            CHECK(at->offset == cut);
            ContentStream again(HEADER_KEY, hash, b.nca.size(), nullptr, write);
            CHECK(again.resume_at(*at));
            CHECK(again.feed(std::span(b.nca).subspan(cut)));
            CHECK(again.finish());
            CHECK(out == whole.out);
        }
    }
    write_seed("nca_packed", build_packed().nca);
    write_seed("nca_packed_tiny", build_packed(0, true).nca);
}

// A header that says a section starts inside the header itself is still refused.
void test_section_inside_header()
{
    Built b = build_packed();
    nca_header_crypt(b.nca.data(), HEADER_KEY, false);
    put_le32(b.nca.data() + 0x240, (uint32_t)(0xa00 / 0x200));
    nca_header_crypt(b.nca.data(), HEADER_KEY, true);
    CHECK(!run(b.nca, std::nullopt, 0).ok);
}

}   // namespace

// NXPress's NCZSECT3 and any later NCZSECT… (7th byte of the magic other than N or 2):
// refused at once, said as such, never written as a plain NCA that fails its hash.
void test_nxpress_extensions()
{
    const Built b = build();
    const auto  hash = sha(b.nca);
    for (const char last : {'3', 'A'}) {
        Bytes ext = ncz_stream(b);
        ext[NCA_FULL_HEADER + 7] = (uint8_t)last;
        if (last == '3') write_seed("ncz_sect3", ext);
        const Result r = run(ext, hash, b.nca.size());
        CHECK(!r.ok && r.error.find("NXPress") != std::string::npos);
        CHECK(r.out.size() <= NCA_FULL_HEADER);   // the header at most, none of the body
        for (const auto& sizes : std::vector<std::vector<size_t>>{{1}, {7, 3}})
            CHECK(!run(ext, hash, b.nca.size(), sizes).ok);
    }
    // Another magic is still a plain NCA's body, and fails on its hash.
    Bytes other = b.nca;
    other[NCA_FULL_HEADER] = 'X';
    const Result r = run(other, hash, b.nca.size());
    CHECK(!r.ok && r.error.find("NXPress") == std::string::npos);
}

namespace
{
// NCZSECT2 (NXPress): the entry's +0x18 is the counter offset of a CTR section, added to a
// byte's position to find its counter (a sparse base's pieces were encrypted at their
// virtual position, not where they are stored).
Bytes ncz_sect2(const Built& b, const int64_t offsets[3])
{
    Bytes z = ncz_stream(b);
    put_le64(z.data() + NCA_FULL_HEADER, 0x32544345535a434eull);   // "NCZSECT2"
    for (int i = 0; i < 3; i++)
        put_le64(z.data() + NCA_FULL_HEADER + 16 + 0x40 * i + 0x18, (uint64_t)offsets[i]);
    return z;
}

// What the NCA is once section 1's CTR is taken at `offset` more than its position.
Bytes shifted(const Built& b, int64_t offset)
{
    Bytes nca = b.nca;
    std::copy(b.plain.begin() + (long)(b.s1 - NCA_FULL_HEADER), b.plain.begin() + (long)(b.s1 - NCA_FULL_HEADER + b.s1_size),
              nca.begin() + (long)b.s1);
    Aes128Ctr(SECTION_KEY, iv_of(CTR1).data()).crypt(std::span(nca).subspan(b.s1, b.s1_size), (uint64_t)((int64_t)b.s1 + offset));
    return nca;
}
}   // namespace

void test_ncz_sect2()
{
    const Built b = build();
    for (const int64_t off : {(int64_t)0x2000, (int64_t)-0x3000, (int64_t)0x10}) {
        const int64_t offsets[3] = {0, off, 0};
        const Bytes   z = ncz_sect2(b, offsets);
        const Bytes   want = shifted(b, off);
        CHECK(want != b.nca);
        if (off == 0x2000) write_seed("ncz_sect2", z);
        for (const auto& sizes : std::vector<std::vector<size_t>>{{0}, {1}, {3, 1000, 17}, {0x4000, 8, 0xc0, 4}}) {
            const Result r = run(z, sha(want), want.size(), sizes);
            CHECK(r.ok && r.compressed && r.out == want);
        }
        // The published hash is the one the NCA had with the counter at its position: a wrong
        // offset is caught by the hash, never installed.
        CHECK(!run(z, sha(b.nca), b.nca.size()).ok);
    }
    // All zero: the same NCA as NCZSECTN gives.
    const int64_t none[3] = {0, 0, 0};
    const Result  same = run(ncz_sect2(b, none), sha(b.nca), b.nca.size());
    CHECK(same.ok && same.out == b.nca);
    // NCZSECTN ignores the field (nsz writes it as padding).
    Bytes plain = ncz_stream(b);
    put_le64(plain.data() + NCA_FULL_HEADER + 16 + 0x18, 0x1230);
    CHECK(run(plain, sha(b.nca), b.nca.size()).out == b.nca);
    // Refused: not a multiple of 16; on a section that is not CTR; before the NCA's start.
    for (const auto& bad : {std::array<int64_t, 3>{0, 8, 0}, {0, 0, 0x10}, {-0x4010, 0, 0}, {0, (int64_t)1 << 62, 0},
                            {0, -((int64_t)b.s1 + 0x10), 0}}) {
        const Result r = run(ncz_sect2(b, bad.data()), sha(b.nca), b.nca.size());
        CHECK(!r.ok && r.error.find("counter offset") != std::string::npos);
    }
}

int main()
{
    test_plain();
    test_resume();
    test_game_card();
    test_ncz();
    test_nxpress_extensions();
    test_ncz_sect2();
    test_packed_layout();
    test_section_inside_header();
    CHECK_DONE("content stream: NCA, NCZ (stream and blocks), game card header, a .nca taken up again, refusals passed");
}
