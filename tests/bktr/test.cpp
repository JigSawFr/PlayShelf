// install/bktr.cpp: an update's RomFS section over a base game's, built here
// (changed, moved and added stretches; subsections of two generations, one not
// encrypted), read back as the game sees it: all of it, by ranges, and against
// the integrity tree the update's header describes (the check that does not
// depend on this reader being right). And what is refused: damaged headers,
// tables that overrun, entries out of order, a storage that does not exist, a
// section decrypted with the wrong generation.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#include "../install/patch_builders.hpp"
#include "check.h"

using namespace install;
using namespace test_install;

namespace
{
bktr::PatchSource source(const Update& u, const Bytes& nca, const Bytes& base)
{
    bktr::PatchSource s;
    s.nca = [&nca](uint64_t off, std::span<uint8_t> out) {
        if (off > nca.size() || out.size() > nca.size() - off) return false;
        std::memcpy(out.data(), nca.data() + off, out.size());
        return true;
    };
    s.section_start = u.start;
    s.section_size = u.section.size();
    std::memcpy(s.key.data(), SECTION_KEY, 16);
    s.upper_iv = u.upper_iv;
    s.encrypted = true;
    auto pi = bktr::read_patch_info(u.fs_header.data(), u.section.size());
    if (pi) s.info = *pi;
    s.base = [&base](uint64_t off, std::span<uint8_t> out) {
        if (off > base.size() || out.size() > base.size() - off) return false;
        std::memcpy(out.data(), base.data() + off, out.size());
        return true;
    };
    s.base_size = base.size();
    return s;
}

// An NCA-sized buffer with the update's section at its place.
Bytes nca_of(const Update& u)
{
    Bytes n(u.start + u.section.size(), 0);
    std::memcpy(n.data() + u.start, u.section.data(), u.section.size());
    return n;
}

void test_reads()
{
    for (size_t image_size : {size_t(200'000), size_t(70'000), size_t(1'000'000)}) {
        const Bytes  base = romfs_image(image_size, (uint32_t)image_size);
        const Update u = make_update(base, 0x4000, 7);
        const Bytes  nca = nca_of(u);
        auto         ps = bktr::PatchedSection::open(source(u, nca, u.base_section));
        if (!CHECK(ps.has_value())) {
            fprintf(stderr, "open: %s\n", ps.error().c_str());
            continue;
        }
        CHECK(ps->size() == u.virtual_section.size());
        CHECK(ps->relocations().count() > 2);
        CHECK(ps->subsections().count() >= 3);
        Bytes all(ps->size());
        CHECK(ps->read(0, all));
        CHECK(all == u.virtual_section);

        // By ranges, across entries and subsections.
        std::mt19937 rng((uint32_t)image_size);
        for (int i = 0; i < 400; i++) {
            const size_t off = rng() % all.size(), len = std::min<size_t>(all.size() - off, rng() % 20000);
            Bytes        got(len);
            if (!CHECK(ps->read(off, got))) break;
            CHECK(std::equal(got.begin(), got.end(), all.begin() + (long)off));
        }
        { Bytes one(1); CHECK(ps->read(all.size() - 1, one)); CHECK(!ps->read(all.size(), one)); }
        { Bytes two(2); CHECK(!ps->read(all.size() - 1, two)); }

        // The update's own integrity tree describes what was read: the check that does not rest on this reader.
        auto layout = ivfc::read_fs_header(u.fs_header.data());
        if (CHECK(layout.has_value()))
            CHECK(ivfc::verify(*layout, [&](uint64_t off, std::span<uint8_t> out) { return ps->read(off, out); }).has_value());
    }
}

// The same section put at another place in its NCA reads the same (the counters follow the place).
void test_other_place()
{
    const Bytes  base = romfs_image(150'000, 3);
    const Update a = make_update(base, 0x4000, 5), b = make_update(base, 0x12200, 5);
    CHECK(a.section != b.section);   // encrypted at other counters
    for (const Update* u : {&a, &b}) {
        const Bytes nca = nca_of(*u);
        auto        ps = bktr::PatchedSection::open(source(*u, nca, u->base_section));
        if (!CHECK(ps.has_value())) continue;
        Bytes all(ps->size());
        CHECK(ps->read(0, all) && all == u->virtual_section);
    }
}

void test_refusals()
{
    const Bytes  base = romfs_image(200'000, 11);
    const Update u = make_update(base, 0x4000, 9);
    const Bytes  nca = nca_of(u);
    CHECK(bktr::PatchedSection::open(source(u, nca, u.base_section)).has_value());
    const auto info = *bktr::read_patch_info(u.fs_header.data(), u.section.size());

    auto fs_refused = [&](const char* what, auto&& edit) {
        Update t = u;
        edit(t.fs_header.data());
        const bool refused = !bktr::read_patch_info(t.fs_header.data(), t.section.size()).has_value();
        if (!refused) fprintf(stderr, "not refused: %s\n", what);
        CHECK(refused);
    };
    fs_refused("no relocation table", [](uint8_t* fs) { put_le64(fs + 0x108, 0); });
    fs_refused("no subsection table", [](uint8_t* fs) { put_le64(fs + 0x128, 0); });
    fs_refused("indirect header magic", [](uint8_t* fs) { fs[0x110] = 'X'; });
    fs_refused("aes header version", [](uint8_t* fs) { fs[0x134] = 9; });
    fs_refused("indirect count 0", [](uint8_t* fs) { put_le32(fs + 0x118, 0); });
    fs_refused("indirect offset misaligned", [](uint8_t* fs) { put_le64(fs + 0x100, le64(fs + 0x100) + 1); });
    fs_refused("indirect past the subsection table", [](uint8_t* fs) { put_le64(fs + 0x108, le64(fs + 0x108) + 0x4000); });
    fs_refused("subsection table past the section", [](uint8_t* fs) { put_le64(fs + 0x120, 1ull << 40); });
    fs_refused("table smaller than its entries", [](uint8_t* fs) { put_le32(fs + 0x118, 100000); });

    // Damage to the stored tables: the subsection table first (plain CTR), then the relocation table (by generation).
    auto nca_refused = [&](const char* what, auto&& edit) {
        Bytes n = nca;
        edit(n);
        auto       ps = bktr::PatchedSection::open(source(u, n, u.base_section));
        bool       refused = !ps.has_value();
        if (!refused) {   // opened: then it must not verify
            Bytes all(ps->size());
            auto  layout = ivfc::read_fs_header(u.fs_header.data());
            refused = !ps->read(0, all) || !ivfc::verify(*layout, [&](uint64_t off, std::span<uint8_t> out) { return ps->read(off, out); }).has_value();
        }
        if (!refused) fprintf(stderr, "not found: %s\n", what);
        CHECK(refused);
    };
    nca_refused("subsection table byte", [&](Bytes& n) { n[u.start + info.aes_offset + 0x4000 + 0x20] ^= 0x10; });
    nca_refused("subsection table header", [&](Bytes& n) { n[u.start + info.aes_offset + 4] ^= 1; });
    nca_refused("relocation table byte", [&](Bytes& n) { n[u.start + info.indirect_offset + 0x4000 + 0x20] ^= 0x10; });
    nca_refused("relocation table header", [&](Bytes& n) { n[u.start + info.indirect_offset + 8] ^= 1; });
    nca_refused("a byte of patch data", [&](Bytes& n) { n[u.start + 5] ^= 1; });
    nca_refused("a byte in the middle of the patch data", [&](Bytes& n) { n[u.start + info.indirect_offset / 2] ^= 1; });

    // A base that is not the update's base: the integrity tree finds it.
    {
        Bytes other = u.base_section;
        other[other.size() / 2] ^= 1;
        auto ps = bktr::PatchedSection::open(source(u, nca, other));
        if (CHECK(ps.has_value())) {
            Bytes all(ps->size());
            auto  layout = ivfc::read_fs_header(u.fs_header.data());
            CHECK(ps->read(0, all));
            CHECK(!ivfc::verify(*layout, [&](uint64_t off, std::span<uint8_t> out) { return ps->read(off, out); }).has_value());
        }
        // A base too short for the entries that point into it: a read fails.
        const Bytes shorter(u.base_section.begin(), u.base_section.begin() + (long)u.base_section.size() / 2);
        auto        ps2 = bktr::PatchedSection::open(source(u, nca, shorter));
        if (CHECK(ps2.has_value())) {
            Bytes all(ps2->size());
            CHECK(!ps2->read(0, all));
        }
    }
    // Another key, or a generation not in the table: the relocation table does not parse.
    {
        auto s = source(u, nca, u.base_section);
        s.key[0] ^= 1;
        CHECK(!bktr::PatchedSection::open(s).has_value());
        auto t = source(u, nca, u.base_section);
        t.upper_iv ^= 1ull << 40;
        CHECK(!bktr::PatchedSection::open(t).has_value());
    }
    CHECK(!bktr::PatchedSection::open(bktr::PatchSource{}).has_value());
    // Trees on their own.
    {
        std::vector<Bytes> es;
        for (int i = 0; i < 5; i++) {
            Bytes e(bktr::SUBSECTION_ENTRY, 0);
            put_le64(e.data(), (uint64_t)i * 64);
            es.push_back(e);
        }
        const Bytes t = tree_table(es, 320, bktr::SUBSECTION_ENTRY);
        CHECK(bktr::parse_tree(t, 5, bktr::SUBSECTION_ENTRY).has_value());
        CHECK(!bktr::parse_tree(t, 6, bktr::SUBSECTION_ENTRY).has_value());
        CHECK(!bktr::parse_tree(t, 0, bktr::SUBSECTION_ENTRY).has_value());
        CHECK(!bktr::parse_tree(std::span<const uint8_t>(t.data(), t.size() - 1), 5, bktr::SUBSECTION_ENTRY).has_value());
        Bytes bad = t;
        put_le64(bad.data() + bktr::NODE_SIZE + bktr::NODE_HEADER + 2 * 0x10, 0);   // entry 2 not after entry 1
        CHECK(!bktr::parse_tree(bad, 5, bktr::SUBSECTION_ENTRY).has_value());
        bad = t;
        put_le64(bad.data() + 8, 64);   // the end before the last entry
        CHECK(!bktr::parse_tree(bad, 5, bktr::SUBSECTION_ENTRY).has_value());
    }
}
}   // namespace

int main()
{
    test_reads();
    test_other_place();
    test_refusals();
    CHECK_DONE("bktr: ok");
}
