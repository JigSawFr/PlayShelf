// install/recompress_meta.cpp: a content meta with its program replaced
// (id, size, hash, the system version raised, the rest untouched, the same
// length) and a .cnmt.nca built here (hash table, master hash, CTR) with that
// content meta put in: the NCA keeps its size and layout, its content meta
// reads back as the new one, the hash table and every hash above it match what
// is computed from scratch here. And what is refused.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#include "../install/builders.hpp"
#include "check.h"
#include "install/aes.hpp"
#include "install/recompress_meta.hpp"

using namespace install;
using namespace test_install;

namespace
{
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

void test_content_ids()
{
    std::array<uint8_t, 32> h{};
    for (int i = 0; i < 32; i++) h[i] = (uint8_t)i;
    const ContentId id = content_id_of(h);
    CHECK(std::memcmp(id.data(), h.data(), 16) == 0);
}

void test_cnmt()
{
    const Bytes in = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, contents());
    auto        out = cnmt_with_program(in, id_of(0x11), now());
    if (!CHECK(out.has_value())) return;
    CHECK(out->size() == in.size());
    auto m = cnmt_parse(*out);
    if (!CHECK(m.has_value())) return;
    CHECK(m->contents.size() == 2);
    CHECK(m->contents[0].id == id_of(0x99) && m->contents[0].hash == now().hash && m->contents[0].size == now().size);
    CHECK(m->contents[0].type == ContentType::Program);
    CHECK(m->contents[1].id == id_of(0x22) && m->contents[1].size == 0x40000 && m->contents[1].hash[0] == 0xbb);
    CHECK(m->required_system_version() == SYSTEM_VERSION_12_0_0);
    // Everything but the program's entry and the version is the same bytes.
    size_t differing = 0;
    for (size_t i = 0; i < in.size(); i++) differing += in[i] != (*out)[i];
    CHECK(differing > 0 && differing < 80);

    // A version already high stays.
    Bytes high = in;
    put_le32(high.data() + 0x20 + 8, 0x40000000);
    auto kept = cnmt_with_program(high, id_of(0x11), now());
    if (CHECK(kept.has_value())) CHECK(le32(kept->data() + 0x28) == 0x40000000);

    // A patch too.
    const Bytes patch = packaged_cnmt(MetaType::Patch, 0x0100000000010800ull, 65536, contents(), Bytes(0));
    auto        pout = cnmt_with_program(patch, id_of(0x11), now());
    if (CHECK(pout.has_value())) CHECK(le32(pout->data() + 0x28) == SYSTEM_VERSION_12_0_0);

    CHECK(!cnmt_with_program(in, id_of(0x77), now()).has_value());   // not in it
    CHECK(!cnmt_with_program(in, id_of(0x22), now()).has_value());   // not a program
    CHECK(!cnmt_with_program(Bytes(10), id_of(0x11), now()).has_value());
    {   // listed twice
        auto twice = contents();
        twice.push_back(twice[0]);
        const Bytes dup = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, twice);
        CHECK(!cnmt_with_program(dup, id_of(0x11), now()).has_value());
    }
    {   // cut short
        Bytes cut = in;
        cut.resize(0x40);
        CHECK(!cnmt_with_program(cut, id_of(0x11), now()).has_value());
    }
}

void test_meta_nca()
{
    const Bytes cnmt = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, contents());
    const Bytes patched = *cnmt_with_program(cnmt, id_of(0x11), now());
    for (uint32_t block : {0x200u, 0x1000u, 0x10000u}) {
        const Bytes nca = meta_nca(cnmt, block);
        auto        out = meta_nca_with_cnmt(nca, patched, test_keys());
        if (!CHECK(out.has_value())) continue;
        CHECK(out->size() == nca.size());
        CHECK(*out != nca);
        auto read = nca_read_cnmt(*out, test_keys());   // header parses (fs header hash included), the cnmt is the new one
        if (CHECK(read.has_value())) CHECK(*read == patched);
        CHECK(nca_read_cnmt(nca, test_keys()).value() == cnmt);   // the input is unchanged

        // The hash table, from scratch: the NCA built around the new content meta is the same bytes.
        CHECK(*out == meta_nca(patched, block));
    }
    // A game card one stays as it is (the distribution is not this module's).
    {
        auto out = meta_nca_with_cnmt(meta_nca(cnmt, 0x1000, 1), patched, test_keys());
        CHECK(out.has_value());
    }
    // Refusals.
    CHECK(!meta_nca_with_cnmt(meta_nca(cnmt), Bytes(patched.begin(), patched.end() - 1), test_keys()).has_value());
    CHECK(!meta_nca_with_cnmt(Bytes(100), patched, test_keys()).has_value());
    NcaKeys wrong = test_keys();
    wrong.header_key[3] ^= 1;
    CHECK(!meta_nca_with_cnmt(meta_nca(cnmt), patched, wrong).has_value());
    NcaKeys nokey = test_keys();
    nokey.unwrap = [](uint8_t, uint8_t, const uint8_t*, uint8_t*) { return false; };
    CHECK(!meta_nca_with_cnmt(meta_nca(cnmt), patched, nokey).has_value());
    {   // a block size of 0, a table that is not the partition's, a content meta file twice
        auto edit = [&](auto&& f) {
            Bytes n = meta_nca(cnmt);
            nca_header_crypt(n.data(), HEADER_KEY, false);
            f(n.data() + 0x400);
            Sha256 sh;
            sh.update({n.data() + 0x400, 0x200});
            const auto d = sh.digest();
            std::memcpy(n.data() + 0x280, d.data(), 32);
            nca_header_crypt(n.data(), HEADER_KEY, true);
            return !meta_nca_with_cnmt(n, patched, test_keys()).has_value();
        };
        CHECK(edit([](uint8_t* fs) { put_le32(fs + 0x28, 0); }));
        CHECK(edit([](uint8_t* fs) { put_le64(fs + 0x38, le64(fs + 0x38) + 32); }));
        CHECK(edit([](uint8_t* fs) { put_le64(fs + 0x48, 1ull << 40); }));
        CHECK(edit([](uint8_t* fs) { fs[3] = 3; }));
    }
}
}   // namespace

int main()
{
    test_content_ids();
    test_cnmt();
    test_meta_nca();
    CHECK_DONE("recompress_meta: ok");
}
