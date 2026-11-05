// install/bktr.hpp: the patch layers of an update's RomFS section.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/bktr.hpp"

#include <algorithm>
#include <cstring>

#include "install/aes.hpp"

namespace install::bktr
{

namespace
{
constexpr uint32_t MAGIC = 0x52544b42;   // "BKTR"
constexpr uint32_t OFFSETS_PER_NODE = (NODE_SIZE - NODE_HEADER) / 8;
constexpr uint64_t MAX_TABLE = 256u << 20;

uint64_t up16(uint64_t v) { return (v + 15) & ~15ull; }

TreeHeader read_header(const uint8_t* p)
{
    return {le32(p), le32(p + 4), (int32_t)le32(p + 8)};
}

bool header_ok(const TreeHeader& h) { return h.magic == MAGIC && h.version <= 1 && h.entry_count > 0; }

std::array<uint8_t, 16> iv_of(uint64_t upper)
{
    NcaSection s;
    s.ctr = upper;
    return nca_section_iv(s);
}
}   // namespace

uint64_t table_size(uint32_t entry_count, size_t entry_size)
{
    const uint32_t per_set = (NODE_SIZE - NODE_HEADER) / (uint32_t)entry_size;
    const uint32_t sets = (entry_count + per_set - 1) / per_set;
    return (uint64_t)NODE_SIZE * (1 + sets);
}

Parsed<Tree> parse_tree(std::span<const uint8_t> table, int32_t entry_count, size_t entry_size)
{
    if (entry_count <= 0) return fail("a patch table with no entries");
    const uint32_t per_set = (NODE_SIZE - NODE_HEADER) / (uint32_t)entry_size, n = (uint32_t)entry_count;
    const uint32_t sets = (n + per_set - 1) / per_set;
    if (sets > OFFSETS_PER_NODE) return fail("a patch table with more entry sets than one node indexes");
    if (table.size() < table_size(n, entry_size)) return fail("a patch table smaller than its entries need");

    const uint8_t* l1 = table.data();
    const int64_t  end = (int64_t)le64(l1 + 8);
    if ((int32_t)le32(l1) != 0 || le32(l1 + 4) != sets || end <= 0) return fail("a damaged patch table header");

    Tree t;
    t.entry_size = entry_size;
    t.end = (uint64_t)end;
    t.entries.reserve((size_t)n * entry_size);
    for (uint32_t s = 0; s < sets; s++) {
        const uint8_t* node = table.data() + (size_t)NODE_SIZE * (1 + s);
        const uint32_t first = s * per_set, count = std::min(per_set, n - first);
        if ((int32_t)le32(node) != (int32_t)s || le32(node + 4) != count) return fail("a damaged patch entry set");
        t.entries.insert(t.entries.end(), node + NODE_HEADER, node + NODE_HEADER + (size_t)count * entry_size);
        if ((int64_t)le64(l1 + NODE_HEADER + 8 * s) != t.offset(first)) return fail("a patch table whose node offsets differ from its entries");
    }
    for (uint32_t s = 0; s < sets; s++) {
        const uint8_t* node = table.data() + (size_t)NODE_SIZE * (1 + s);
        const int64_t  want = s + 1 < sets ? t.offset((size_t)(s + 1) * per_set) : end;
        if ((int64_t)le64(node + 8) != want) return fail("a patch entry set that ends elsewhere");
    }
    for (uint32_t i = 0; i < n; i++) {
        const int64_t next = i + 1 < n ? t.offset(i + 1) : end;
        if ((i == 0 && t.offset(0) != 0) || t.offset(i) < 0 || next <= t.offset(i) || next > end) return fail("patch entries out of order");
    }
    return t;
}

Parsed<PatchInfo> read_patch_info(const uint8_t* fs, uint64_t section_size)
{
    PatchInfo p;
    p.indirect_offset = le64(fs + PATCH_INFO_AT);
    p.indirect_size = le64(fs + PATCH_INFO_AT + 8);
    p.indirect = read_header(fs + PATCH_INFO_AT + 0x10);
    p.aes_offset = le64(fs + PATCH_INFO_AT + 0x20);
    p.aes_size = le64(fs + PATCH_INFO_AT + 0x28);
    p.aes = read_header(fs + PATCH_INFO_AT + 0x30);
    if (!p.has_indirect() || !p.has_aes()) return fail("a patch section without both its tables");
    if (!header_ok(p.indirect) || !header_ok(p.aes)) return fail("a patch table header that is not one");
    if (p.indirect_offset % 16 != 0 || p.indirect_offset > section_size || p.indirect_size > section_size - p.indirect_offset)
        return fail("the relocation table outside its section");
    if (p.indirect_offset + p.indirect_size > p.aes_offset || p.aes_offset > section_size || up16(p.aes_size) > section_size - p.aes_offset)
        return fail("the subsection table outside its section");
    if (p.indirect_size > MAX_TABLE || p.aes_size > MAX_TABLE) return fail("a patch table too big");
    if (table_size((uint32_t)p.indirect.entry_count, RELOCATION_ENTRY) > p.indirect_size ||
        table_size((uint32_t)p.aes.entry_count, SUBSECTION_ENTRY) > p.aes_size)
        return fail("a patch table smaller than its entries need");
    return p;
}

Parsed<PatchedSection> PatchedSection::open(PatchSource src)
{
    const PatchInfo& pi = src.info;
    if (!src.nca || !src.base) return fail("no source for a patched section");

    PatchedSection ps;
    ps.src = std::move(src);
    PatchSource& s = ps.src;

    // The subsection table: plain CTR, at the place the patch info gives.
    std::vector<uint8_t> aes_table((size_t)up16(pi.aes_size));
    if (!s.nca(s.section_start + pi.aes_offset, aes_table)) return fail("the subsection table could not be read");
    if (s.encrypted) {
        const auto iv = iv_of(s.upper_iv);
        Aes128Ctr(s.key.data(), iv.data()).crypt(aes_table, s.section_start + pi.aes_offset);
    }
    auto aes = parse_tree(aes_table, pi.aes.entry_count, SUBSECTION_ENTRY);
    if (!aes) return fail("the subsection table: " + aes.error());
    for (size_t i = 0; i < aes->count(); i++) {
        const uint8_t* e = aes->at(i);
        if (aes->offset(i) % 16 != 0 || e[8] > 1) return fail("a subsection entry that is not valid");
    }
    if (aes->end < pi.indirect_offset + pi.indirect_size) return fail("the subsection table does not cover the relocation table");
    ps.aes = std::move(*aes);

    // The relocation table: inside the patch data, decrypted by the subsections.
    std::vector<uint8_t> table((size_t)pi.indirect_size);
    if (!ps.read_patch(pi.indirect_offset, table)) return fail("the relocation table could not be read");
    auto ind = parse_tree(table, pi.indirect.entry_count, RELOCATION_ENTRY);
    if (!ind) return fail("the relocation table: " + ind.error());
    for (size_t i = 0; i < ind->count(); i++) {
        const uint8_t* e = ind->at(i);
        const int32_t  storage = (int32_t)le32(e + 16);
        if ((int64_t)le64(e + 8) < 0 || (storage != 0 && storage != 1)) return fail("a relocation entry that is not valid");
    }
    ps.virt_size = ind->end;
    ps.indirect = std::move(*ind);
    return ps;
}

bool PatchedSection::read_patch(uint64_t offset, std::span<uint8_t> out) const
{
    if (offset > aes.end || out.size() > aes.end - offset) return false;
    const uint32_t secure = (uint32_t)(src.upper_iv >> 32);
    // The subsection holding `offset`.
    size_t lo = 0, hi = aes.count();
    while (hi - lo > 1) {
        const size_t mid = (lo + hi) / 2;
        if ((uint64_t)aes.offset(mid) <= offset) lo = mid;
        else hi = mid;
    }
    for (size_t i = lo; !out.empty(); i++) {
        if (i >= aes.count()) return false;
        const uint64_t next = i + 1 < aes.count() ? (uint64_t)aes.offset(i + 1) : aes.end;
        const size_t   take = (size_t)std::min<uint64_t>(out.size(), next - offset);
        if (!src.nca(src.section_start + offset, out.first(take))) return false;
        if (src.encrypted && aes.at(i)[8] == 0) {
            const uint64_t upper = (uint64_t)secure << 32 | le32(aes.at(i) + 12);
            const auto     iv = iv_of(upper);
            Aes128Ctr(src.key.data(), iv.data()).crypt(out.first(take), src.section_start + offset);
        }
        out = out.subspan(take);
        offset += take;
    }
    return true;
}

bool PatchedSection::read(uint64_t offset, std::span<uint8_t> out) const
{
    if (offset > virt_size || out.size() > virt_size - offset) return false;
    size_t lo = 0, hi = indirect.count();
    while (hi - lo > 1) {
        const size_t mid = (lo + hi) / 2;
        if ((uint64_t)indirect.offset(mid) <= offset) lo = mid;
        else hi = mid;
    }
    for (size_t i = lo; !out.empty(); i++) {
        if (i >= indirect.count()) return false;
        const uint8_t* e = indirect.at(i);
        const uint64_t next = i + 1 < indirect.count() ? (uint64_t)indirect.offset(i + 1) : virt_size;
        const uint64_t within = offset - (uint64_t)indirect.offset(i), phys = le64(e + 8) + within;
        const size_t   take = (size_t)std::min<uint64_t>(out.size(), next - offset);
        if (le32(e + 16) == 0) {
            if (phys > src.base_size || take > src.base_size - phys || !src.base(phys, out.first(take))) return false;
        } else {
            if (phys > src.info.indirect_offset || take > src.info.indirect_offset - phys || !read_patch(phys, out.first(take))) return false;
        }
        out = out.subspan(take);
        offset += take;
    }
    return true;
}

}   // namespace install::bktr
