// Inputs the patch suites build: a base game's RomFS section, and an update's
// section over it (patch data, relocation table, subsection table), with the
// integrity tree over what the game sees. Self-consistent with the reader, not
// a console's: the layout follows the format's description.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <map>

#include "recompress_builders.hpp"
#include "install/bktr.hpp"

namespace test_install
{

// A bucket tree's table for entries of a size (each entry's first 8 bytes are its offset).
inline Bytes tree_table(const std::vector<Bytes>& entries, uint64_t end, size_t entry_size)
{
    const uint32_t per_set = (bktr::NODE_SIZE - bktr::NODE_HEADER) / (uint32_t)entry_size;
    const uint32_t n = (uint32_t)entries.size(), sets = (n + per_set - 1) / per_set;
    Bytes          t(bktr::table_size(n, entry_size), 0);
    put_le32(t.data() + 4, sets);
    put_le64(t.data() + 8, end);
    for (uint32_t s = 0; s < sets; s++) {
        uint8_t*       node = t.data() + (size_t)bktr::NODE_SIZE * (1 + s);
        const uint32_t first = s * per_set, count = std::min(per_set, n - first);
        put_le32(node, s);
        put_le32(node + 4, count);
        put_le64(node + 8, first + count < n ? le64(entries[first + count].data()) : end);
        put_le64(t.data() + bktr::NODE_HEADER + 8 * s, le64(entries[first].data()));
        for (uint32_t i = 0; i < count; i++) std::memcpy(node + bktr::NODE_HEADER + (size_t)i * entry_size, entries[first + i].data(), entry_size);
    }
    return t;
}

struct Update
{
    Bytes    base_section;      // the base game's section, decrypted: hash levels, then the data
    Bytes    virtual_section;   // what the game sees with the update on: the same, patched
    Bytes    section;           // the update's section as stored (patch data, tables, encrypted)
    Bytes    patched_image;     // the RomFS image of virtual_section
    std::array<uint8_t, 0x200> fs_header{};   // the update's file system header for it
    uint64_t upper_iv = 0;
    uint64_t start = 0;         // the section's place in an NCA it is put in
    ivfc::Layout layout;        // of virtual_section
};

// A section of the shape program_nca() makes (hash levels first, the data after).
inline Bytes ivfc_section(const Bytes& image, ivfc::Layout* layout)
{
    ivfc::Builder tree;
    tree.add(image);
    auto         r = tree.finish();
    Bytes        s = r.hash_levels;
    const uint64_t prefix = r.hash_levels.size();
    s.insert(s.end(), image.begin(), image.end());
    s.insert(s.end(), (0x4000 - image.size() % 0x4000) % 0x4000, 0);
    ivfc::Layout l = r.layout;
    for (size_t i = 0; i < 5; i++) l.offset[i] -= image.size();
    l.offset[5] = prefix;
    *layout = l;
    s.resize((s.size() + 0x1ff) / 0x200 * 0x200);
    return s;
}

// `base_image` patched: a stretch changed, a stretch moved from elsewhere, some
// added at the end. `start`: where the update's section stands in its NCA.
inline Update make_update(const Bytes& base_image, uint64_t start, uint32_t seed, const uint8_t* key = SECTION_KEY)
{
    Update u;
    u.start = start;
    ivfc::Layout base_layout;
    u.base_section = ivfc_section(base_image, &base_layout);

    Bytes p = base_image;
    std::mt19937 rng(seed);
    for (size_t i = 0; i < 20000 && i + 30000 < p.size(); i++) p[30000 + i] = (uint8_t)rng();   // changed
    if (p.size() > 120000)
        std::copy(base_image.begin() + 8192, base_image.begin() + 8192 + 20480, p.begin() + 90112);   // a block-aligned stretch from elsewhere
    for (size_t i = 0; i < 9000; i++) p.push_back((uint8_t)("added to the end "[i % 17]));                // added
    u.patched_image = p;
    u.virtual_section = ivfc_section(p, &u.layout);

    // Relocations, by blocks of 4 KiB: from the base where a block is in it, else from the patch.
    constexpr size_t G = 0x1000;
    std::map<std::string, uint64_t> in_base;
    for (uint64_t off = 0; off + G <= u.base_section.size(); off += G) in_base.emplace(std::string((const char*)&u.base_section[off], G), off);
    struct Run { uint64_t virt, phys; int storage; uint64_t len; };
    std::vector<Run> runs;
    Bytes            patch_data;
    const uint64_t   vsize = u.virtual_section.size();
    for (uint64_t off = 0; off < vsize; off += G) {
        const size_t n = (size_t)std::min<uint64_t>(G, vsize - off);
        Bytes        block(G, 0);
        std::memcpy(block.data(), &u.virtual_section[off], n);
        uint64_t phys;
        int      storage;
        const auto it = n == G ? in_base.find(std::string((const char*)block.data(), G)) : in_base.end();
        if (it != in_base.end()) {
            storage = 0;
            phys = it->second;
        } else {
            storage = 1;
            phys = patch_data.size();
            patch_data.insert(patch_data.end(), block.begin(), block.begin() + (long)n);
            patch_data.resize((patch_data.size() + 15) / 16 * 16);
        }
        if (!runs.empty() && runs.back().storage == storage && runs.back().virt + runs.back().len == off &&
            runs.back().phys + runs.back().len == phys)
            runs.back().len += n;
        else
            runs.push_back({off, phys, storage, n});
    }
    std::vector<Bytes> reloc;
    for (const Run& r : runs) {
        Bytes e(bktr::RELOCATION_ENTRY, 0);
        put_le64(e.data(), r.virt);
        put_le64(e.data() + 8, r.phys);
        put_le32(e.data() + 16, (uint32_t)r.storage);
        reloc.push_back(e);
    }
    const Bytes reloc_table = tree_table(reloc, vsize, bktr::RELOCATION_ENTRY);

    // The patch section: data, relocation table, subsection table.
    const uint64_t indirect_offset = (patch_data.size() + 15) / 16 * 16, indirect_size = reloc_table.size();
    const uint64_t aes_offset = indirect_offset + indirect_size;
    Bytes          body = patch_data;
    body.resize(indirect_offset);
    body.insert(body.end(), reloc_table.begin(), reloc_table.end());
    // Subsections: generations 2 and 5 over the data, one range not encrypted, the table 3.
    const uint64_t cut1 = patch_data.size() / 3 / 16 * 16, cut2 = patch_data.size() * 2 / 3 / 16 * 16;
    struct Sub { uint64_t off; uint8_t enc; uint32_t gen; };
    std::vector<Sub> subs = {{0, 0, 2}, {cut1, 1, 0}, {cut2, 0, 5}, {indirect_offset, 0, 3}};
    if (cut1 == 0) subs.erase(subs.begin() + 1);   // a tiny patch
    std::vector<Bytes> sub_entries;
    for (const Sub& s : subs) {
        Bytes e(bktr::SUBSECTION_ENTRY, 0);
        put_le64(e.data(), s.off);
        e[8] = s.enc;
        put_le32(e.data() + 12, s.gen);
        sub_entries.push_back(e);
    }
    const Bytes aes_table = tree_table(sub_entries, aes_offset, bktr::SUBSECTION_ENTRY);

    u.upper_iv = 0x0000000900000000ull;   // secure value 9
    const uint32_t secure = (uint32_t)(u.upper_iv >> 32);
    for (size_t i = 0; i < subs.size(); i++) {
        if (subs[i].enc) continue;
        const uint64_t from = subs[i].off, to = i + 1 < subs.size() ? subs[i + 1].off : aes_offset;
        NcaSection s;
        s.ctr = (uint64_t)secure << 32 | subs[i].gen;
        const auto iv = nca_section_iv(s);
        Aes128Ctr(key, iv.data()).crypt(std::span(body).subspan(from, to - from), start + from);
    }
    Bytes aes_enc = aes_table;
    {
        NcaSection s;
        s.ctr = u.upper_iv;
        const auto iv = nca_section_iv(s);
        Aes128Ctr(key, iv.data()).crypt(aes_enc, start + aes_offset);
    }
    u.section = body;
    u.section.insert(u.section.end(), aes_enc.begin(), aes_enc.end());
    u.section.resize((u.section.size() + 0x1ff) / 0x200 * 0x200);

    uint8_t* fs = u.fs_header.data();
    put_le16(fs, 2);
    fs[2] = 0;
    fs[3] = 3;
    fs[4] = 4;   // BKTR
    ivfc::write_fs_header(fs, u.layout);
    put_le64(fs + bktr::PATCH_INFO_AT, indirect_offset);
    put_le64(fs + bktr::PATCH_INFO_AT + 8, indirect_size);
    put_le32(fs + bktr::PATCH_INFO_AT + 0x10, 0x52544b42);
    put_le32(fs + bktr::PATCH_INFO_AT + 0x14, 1);
    put_le32(fs + bktr::PATCH_INFO_AT + 0x18, (uint32_t)reloc.size());
    put_le64(fs + bktr::PATCH_INFO_AT + 0x20, aes_offset);
    put_le64(fs + bktr::PATCH_INFO_AT + 0x28, aes_table.size());
    put_le32(fs + bktr::PATCH_INFO_AT + 0x30, 0x52544b42);
    put_le32(fs + bktr::PATCH_INFO_AT + 0x34, 1);
    put_le32(fs + bktr::PATCH_INFO_AT + 0x38, (uint32_t)sub_entries.size());
    put_le64(fs + 0x140, u.upper_iv);
    return u;
}

// A different section key for an update (the key area is still wrapped with KAEK).
inline const uint8_t SECTION_KEY_UPDATE[16] = {0x7a, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

struct UpdateNca
{
    Bytes  nca, exefs_plain;
    Update patch;
};

// The program NCA of an update for `base` (made by program_nca): its ExeFS
// (other bytes), its RomFS section a patch over the base game's, its own section
// key, a program id 0x800 above the base's, another SDK version.
inline UpdateNca update_nca(const Built& base, uint32_t seed, bool rights = false, uint64_t program_id = 0x0100000000020800ull)
{
    UpdateNca u;
    u.exefs_plain = archive(PfsKind::Pfs0, {{"main", stream(7000, "main v2")}, {"main.npdm", stream(1100, "npdm v2")}});
    const size_t hash_table = 0x20;
    Bytes        s0(((hash_table + u.exefs_plain.size() + 0x1ff) / 0x200) * 0x200);
    std::memcpy(s0.data() + hash_table, u.exefs_plain.data(), u.exefs_plain.size());

    const uint64_t s0_start = NCA_FULL_HEADER, s1_start = s0_start + s0.size();
    u.patch = make_update(base.image, s1_start, seed, SECTION_KEY_UPDATE);
    u.nca.assign(s1_start + u.patch.section.size(), 0);
    uint8_t* h = u.nca.data();
    put_le32(h + 0x200, 0x3341434e);
    h[0x204] = 0;
    h[0x205] = 0;
    h[0x206] = 2;
    h[0x220] = 3;
    put_le64(h + 0x208, u.nca.size());
    put_le64(h + 0x210, program_id);
    put_le32(h + 0x21c, 0x00abcdef);
    if (rights) h[0x230] = 0x55;

    uint8_t fs0[0x200] = {};
    put_le16(fs0, 2); fs0[2] = 1; fs0[3] = 2; fs0[4] = 3;
    put_le64(fs0 + 0x40, hash_table);
    put_le64(fs0 + 0x48, u.exefs_plain.size());
    put_le64(fs0 + 0x140, 0x0000000700000000ull);
    put_le32(h + 0x240, (uint32_t)(s0_start / 0x200));
    put_le32(h + 0x244, (uint32_t)(s1_start / 0x200));
    put_le32(h + 0x250, (uint32_t)(s1_start / 0x200));
    put_le32(h + 0x254, (uint32_t)(u.nca.size() / 0x200));
    std::memcpy(h + 0x400, fs0, 0x200);
    std::memcpy(h + 0x600, u.patch.fs_header.data(), 0x200);
    for (int i = 0; i < 2; i++) {
        Sha256 sh;
        sh.update({h + 0x400 + 0x200 * i, 0x200});
        const auto d = sh.digest();
        std::memcpy(h + 0x280 + 0x20 * i, d.data(), 32);
    }
    uint8_t key_area[0x40] = {};
    std::memcpy(key_area + 0x20, SECTION_KEY_UPDATE, 16);
    aes128_ecb_encrypt(KAEK, key_area);
    std::memcpy(h + 0x300, key_area, 0x40);

    std::memcpy(u.nca.data() + s0_start, s0.data(), s0.size());
    NcaSection n0;
    n0.ctr = le64(fs0 + 0x140);
    const auto iv0 = nca_section_iv(n0);
    Aes128Ctr(SECTION_KEY_UPDATE, iv0.data()).crypt(std::span(u.nca).subspan(s0_start, s0.size()), s0_start);
    std::memcpy(u.nca.data() + s1_start, u.patch.section.data(), u.patch.section.size());
    nca_header_crypt(h, HEADER_KEY, true);
    return u;
}

}   // namespace test_install
