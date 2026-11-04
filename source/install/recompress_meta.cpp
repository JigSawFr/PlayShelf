// install/recompress_meta.hpp: the content meta of a recompressed program.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/recompress_meta.hpp"

#include <algorithm>
#include <cstring>

#include "install/aes.hpp"
#include "install/pfs.hpp"
#include "util/sha256.hpp"

namespace install
{

namespace
{
constexpr size_t   HEADER = 0x20, INFO = 0x38;
constexpr uint64_t MAX_META_SECTION = 4u << 20;

std::array<uint8_t, 32> sha(std::span<const uint8_t> b)
{
    Sha256 h;
    h.update(b);
    return h.digest();
}
}   // namespace

ContentId content_id_of(const std::array<uint8_t, 32>& h)
{
    ContentId id;
    std::memcpy(id.data(), h.data(), 16);
    return id;
}

Parsed<std::vector<uint8_t>> cnmt_with_program(std::span<const uint8_t> cnmt, const ContentId& old_id, const ContentInfo& now)
{
    if (cnmt.size() < HEADER) return fail("a content meta cut short");
    const auto meta = cnmt_parse(cnmt);
    if (!meta) return fail(meta.error());
    const size_t ext = le16(cnmt.data() + 0xe), count = le16(cnmt.data() + 0x10);
    if (HEADER + ext + count * INFO > cnmt.size()) return fail("a content meta cut short");

    std::vector<uint8_t> out(cnmt.begin(), cnmt.end());
    bool                 found = false;
    for (size_t i = 0; i < count; i++) {
        uint8_t* e = out.data() + HEADER + ext + i * INFO;
        if (std::memcmp(e + 32, old_id.data(), 16) != 0) continue;
        if (e[54] != (uint8_t)ContentType::Program) return fail("the content to replace is not a program");
        if (found) return fail("a program content listed twice");
        found = true;
        std::memcpy(e, now.hash.data(), 32);
        std::memcpy(e + 32, now.id.data(), 16);
        put_le32(e + 48, (uint32_t)now.size);
        e[52] = (uint8_t)(now.size >> 32);
    }
    if (!found) return fail("the program is not in the content meta");

    // The system version an application or patch requires: its extended
    // header's second field.
    if ((meta->type == MetaType::Application || meta->type == MetaType::Patch) && ext >= 12) {
        uint8_t* v = out.data() + HEADER + 8;
        if (le32(v) < SYSTEM_VERSION_12_0_0) put_le32(v, SYSTEM_VERSION_12_0_0);
    }
    return out;
}

Parsed<std::vector<uint8_t>> meta_nca_with_cnmt(std::span<const uint8_t> nca, std::span<const uint8_t> cnmt, const NcaKeys& keys)
{
    using Bytes = std::vector<uint8_t>;
    if (nca.size() < NCA_FULL_HEADER) return fail("content meta NCA cut short");
    Bytes out(nca.begin(), nca.end());
    uint8_t* head = out.data();
    nca_header_crypt(head, keys.header_key.data(), false);
    const auto h = nca_parse_header(head, out.size());
    if (!h) return fail(h.error());
    if (h->content_type != 1) return fail("not a content meta NCA");
    if (h->has_rights_id()) return fail("a content meta NCA with a rights id");
    const NcaSection& s = h->sections[0];
    if (!s.present || s.fs_type != 1 || s.hash_type != 2) return fail("no SHA-256 partition section");
    if (s.encryption != NcaEncryption::Ctr && s.encryption != NcaEncryption::None) return fail("section encryption other than CTR");
    if (s.end - s.start > MAX_META_SECTION) return fail("a content meta section too big");

    std::array<uint8_t, 0x200> fs = s.fs_header;
    const uint32_t block = le32(fs.data() + 0x28);
    const uint64_t table_at = le64(fs.data() + 0x30), table_size = le64(fs.data() + 0x38);
    const uint64_t pfs_at = le64(fs.data() + 0x40), pfs_size = le64(fs.data() + 0x48);
    const uint64_t section_len = s.end - s.start;
    uint64_t       e1, e2;
    if (block == 0 || !add_ok(table_at, table_size, &e1) || !add_ok(pfs_at, pfs_size, &e2) || e1 > section_len || e2 > section_len ||
        table_size != (pfs_size + block - 1) / block * 32 || (table_at < pfs_at + pfs_size && pfs_at < table_at + table_size))
        return fail("the partition and its hash table are not where the header says");

    Bytes plain(out.begin() + (long)s.start, out.begin() + (long)s.end);
    std::optional<Aes128Ctr> ctr;
    if (s.encryption == NcaEncryption::Ctr) {
        const auto key = nca_ctr_key(*h, keys);
        if (!key) return fail("no key for the section (key generation newer than the console)");
        const auto iv = nca_section_iv(s);
        ctr.emplace(key->data(), iv.data());
        ctr->crypt(plain, s.start);
    }

    const auto p = pfs_parse({plain.data() + pfs_at, (size_t)pfs_size}, pfs_size);
    if (!p) return fail(p.error());
    const auto files = p->ending_with(".cnmt");
    if (files.size() != 1) return fail("not exactly one .cnmt in the partition");
    if (files[0]->size != cnmt.size()) return fail("a content meta of another length");
    std::memcpy(plain.data() + pfs_at + files[0]->offset, cnmt.data(), cnmt.size());

    // The partition's hash table, over blocks of `block` bytes (the last one
    // as short as it is), and the master hash over the table.
    for (uint64_t i = 0, n = table_size / 32; i < n; i++) {
        const uint64_t from = i * block, len = std::min<uint64_t>(block, pfs_size - from);
        const auto     d = sha({plain.data() + pfs_at + from, (size_t)len});
        std::memcpy(plain.data() + table_at + i * 32, d.data(), 32);
    }
    const auto master = sha({plain.data() + table_at, (size_t)table_size});
    std::memcpy(fs.data() + 0x08, master.data(), 32);

    if (ctr) ctr->crypt(plain, s.start);
    std::memcpy(out.data() + s.start, plain.data(), plain.size());

    const auto fs_hash = sha(fs);
    std::memcpy(head + 0x280, fs_hash.data(), 32);
    std::memcpy(head + 0x400, fs.data(), 0x200);
    nca_header_crypt(head, keys.header_key.data(), true);
    return out;
}

}   // namespace install
