// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/nca.hpp"

#include <cstring>

#include "install/aes.hpp"
#include "install/bytes.hpp"
#include "install/pfs.hpp"
#include "install/rsa.hpp"
#include "util/sha256.hpp"

namespace install
{
namespace
{
constexpr uint32_t MAGIC_NCA3 = 0x3341434e;   // "NCA3"

// Nintendo's fixed public key for an NCA header's first signature, key
// generation 0 (the retail one), as the installers it comes from have it.
const uint8_t FIXED_MODULUS_0[256] = {
    0xBF, 0xBE, 0x40, 0x6C, 0xF4, 0xA7, 0x80, 0xE9, 0xF0, 0x7D, 0x0C, 0x99, 0x61, 0x1D, 0x77, 0x2F, 0x96, 0xBC, 0x4B,
    0x9E, 0x58, 0x38, 0x1B, 0x03, 0xAB, 0xB1, 0x75, 0x49, 0x9F, 0x2B, 0x4D, 0x58, 0x34, 0xB0, 0x05, 0xA3, 0x75, 0x22,
    0xBE, 0x1A, 0x3F, 0x03, 0x73, 0xAC, 0x70, 0x68, 0xD1, 0x16, 0xB9, 0x04, 0x46, 0x5E, 0xB7, 0x07, 0x91, 0x2F, 0x07,
    0x8B, 0x26, 0xDE, 0xF6, 0x00, 0x07, 0xB2, 0xB4, 0x51, 0xF8, 0x0D, 0x0A, 0x5E, 0x58, 0xAD, 0xEB, 0xBC, 0x9A, 0xD6,
    0x49, 0xB9, 0x64, 0xEF, 0xA7, 0x82, 0xB5, 0xCF, 0x6D, 0x70, 0x13, 0xB0, 0x0F, 0x85, 0xF6, 0xA9, 0x08, 0xAA, 0x4D,
    0x67, 0x66, 0x87, 0xFA, 0x89, 0xFF, 0x75, 0x90, 0x18, 0x1E, 0x6B, 0x3D, 0xE9, 0x8A, 0x68, 0xC9, 0x26, 0x04, 0xD9,
    0x80, 0xCE, 0x3F, 0x5E, 0x92, 0xCE, 0x01, 0xFF, 0x06, 0x3B, 0xF2, 0xC1, 0xA9, 0x0C, 0xCE, 0x02, 0x6F, 0x16, 0xBC,
    0x92, 0x42, 0x0A, 0x41, 0x64, 0xCD, 0x52, 0xB6, 0x34, 0x4D, 0xAE, 0xC0, 0x2E, 0xDE, 0xA4, 0xDF, 0x27, 0x68, 0x3C,
    0xC1, 0xA0, 0x60, 0xAD, 0x43, 0xF3, 0xFC, 0x86, 0xC1, 0x3E, 0x6C, 0x46, 0xF7, 0x7C, 0x29, 0x9F, 0xFA, 0xFD, 0xF0,
    0xE3, 0xCE, 0x64, 0xE7, 0x35, 0xF2, 0xF6, 0x56, 0x56, 0x6F, 0x6D, 0xF1, 0xE2, 0x42, 0xB0, 0x83, 0x40, 0xA5, 0xC3,
    0x20, 0x2B, 0xCC, 0x9A, 0xAE, 0xCA, 0xED, 0x4D, 0x70, 0x30, 0xA8, 0x70, 0x1C, 0x70, 0xFD, 0x13, 0x63, 0x29, 0x02,
    0x79, 0xEA, 0xD2, 0xA7, 0xAF, 0x35, 0x28, 0x32, 0x1C, 0x7B, 0xE6, 0x2F, 0x1A, 0xAA, 0x40, 0x7E, 0x32, 0x8C, 0x27,
    0x42, 0xFE, 0x82, 0x78, 0xEC, 0x0D, 0xEB, 0xE6, 0x83, 0x4B, 0x6D, 0x81, 0x04, 0x40, 0x1A, 0x9E, 0x9A, 0x67, 0xF6,
    0x72, 0x29, 0xFA, 0x04, 0xF0, 0x9D, 0xE4, 0xF4, 0x03};

}   // namespace

bool NcaHeader::has_rights_id() const
{
    for (uint8_t b : rights_id)
        if (b) return true;
    return false;
}

void nca_header_crypt(uint8_t header[NCA_HEADER], const uint8_t header_key[32], bool encrypt)
{
    aes128_xts_nintendo(header_key, {header, NCA_HEADER}, 0, 0x200, encrypt);
}

Parsed<NcaHeader> nca_parse_header(const uint8_t* d, uint64_t nca_size)
{
    if (le32(d + 0x200) != MAGIC_NCA3) return fail("not an NCA3 header (or the wrong header key)");
    NcaHeader h;
    h.distribution = d[0x204];
    h.content_type = d[0x205];
    h.key_generation = d[0x206] > d[0x220] ? d[0x206] : d[0x220];
    h.key_area_index = d[0x207];
    h.size = le64(d + 0x208);
    h.program_id = le64(d + 0x210);
    h.sdk_version = le32(d + 0x21c);
    h.signature_key_generation = d[0x221];
    std::memcpy(h.rights_id.data(), d + 0x230, 16);
    std::memcpy(h.key_area.data(), d + 0x300, 0x40);
    if (h.distribution > 1) return fail("unknown distribution type");
    if (h.key_area_index > 2) return fail("unknown key area index");
    if (h.size < NCA_FULL_HEADER) return fail("content smaller than its header");
    if (nca_size && h.size != nca_size) return fail("content size differs from its file's");
    for (int i = 0; i < 4; i++) {
        const uint8_t* e = d + 0x240 + 0x10 * i;
        const uint32_t start = le32(e), end = le32(e + 4);
        if (!start && !end) continue;
        NcaSection& s = h.sections[i];
        s.present = true;
        s.start = (uint64_t)start * NCA_MEDIA;
        s.end = (uint64_t)end * NCA_MEDIA;
        if (s.end < s.start || s.start < NCA_FULL_HEADER || s.end > h.size)
            return fail("a section out of the content's bounds");
        std::memcpy(s.fs_header.data(), d + 0x400 + 0x200 * i, 0x200);
        Sha256 sha;
        sha.update(s.fs_header);
        if (std::memcmp(sha.digest().data(), d + 0x280 + 0x20 * i, 32) != 0)
            return fail("a section's fs header does not match its hash");
        s.fs_type = s.fs_header[2];
        s.hash_type = s.fs_header[3];
        s.encryption = (NcaEncryption)s.fs_header[4];
        s.ctr = le64(s.fs_header.data() + 0x140);
    }
    return h;
}

bool nca_signature_valid_with(const uint8_t header[NCA_HEADER], const uint8_t modulus[256])
{
    return rsa2048_pss_sha256_verify({header + 0x200, 0x200}, header, modulus);
}

NcaSignature nca_check_signature(const uint8_t header[NCA_HEADER])
{
    if (header[0x221] != 0) return NcaSignature::UnknownKey;
    return nca_signature_valid_with(header, FIXED_MODULUS_0) ? NcaSignature::Valid : NcaSignature::Invalid;
}

bool nca_make_download(uint8_t header[NCA_HEADER], const uint8_t header_key[32])
{
    uint8_t plain[NCA_HEADER];
    std::memcpy(plain, header, NCA_HEADER);
    nca_header_crypt(plain, header_key, false);
    if (le32(plain + 0x200) != MAGIC_NCA3 || plain[0x204] != 1) return false;
    plain[0x204] = 0;
    nca_header_crypt(plain, header_key, true);
    std::memcpy(header, plain, NCA_HEADER);
    return true;
}

std::optional<std::array<uint8_t, 16>> nca_ctr_key(const NcaHeader& h, const NcaKeys& keys)
{
    if (!keys.unwrap) return std::nullopt;
    std::array<uint8_t, 16> key{};
    if (!keys.unwrap(h.key_area_generation(), h.key_area_index, h.key_area.data() + 0x20, key.data())) return std::nullopt;
    return key;
}

std::array<uint8_t, 16> nca_section_iv(const NcaSection& s)
{
    std::array<uint8_t, 16> iv{};
    for (int i = 0; i < 8; i++)
        iv[i] = (uint8_t)(s.ctr >> (8 * (7 - i)));
    return iv;
}

Parsed<std::vector<uint8_t>> nca_read_cnmt(std::span<const uint8_t> nca, const NcaKeys& keys)
{
    using Bytes = std::vector<uint8_t>;
    if (nca.size() < NCA_FULL_HEADER) return fail("content meta NCA cut short");
    uint8_t head[NCA_HEADER];
    std::memcpy(head, nca.data(), NCA_HEADER);
    nca_header_crypt(head, keys.header_key.data(), false);
    const auto h = nca_parse_header(head, nca.size());
    if (!h) return fail(h.error());
    if (h->content_type != 1) return fail("not a content meta NCA");
    if (h->has_rights_id()) return fail("a content meta NCA with a rights id");
    const NcaSection& s = h->sections[0];
    if (!s.present || s.fs_type != 1) return fail("no partition section");
    if (s.encryption != NcaEncryption::Ctr && s.encryption != NcaEncryption::None)
        return fail("section encryption other than CTR");

    // The PFS0's place in the section (hierarchical SHA-256: its second region).
    const uint64_t pfs_at = le64(s.fs_header.data() + 0x40), pfs_size = le64(s.fs_header.data() + 0x48);
    uint64_t       from, to;
    if (!add_ok(s.start, pfs_at, &from) || !add_ok(from, pfs_size, &to) || to > s.end || pfs_size > (64u << 20))
        return fail("the partition out of its section");
    Bytes pfs(nca.data() + from, nca.data() + to);
    if (s.encryption == NcaEncryption::Ctr) {
        const auto key = nca_ctr_key(*h, keys);
        if (!key) return fail("no key for the section (key generation newer than the console)");
        const auto iv = nca_section_iv(s);
        Aes128Ctr(key->data(), iv.data()).crypt(pfs, from);
    }
    const auto p = pfs_parse(pfs, pfs.size());
    if (!p) return fail(p.error());
    const auto files = p->ending_with(".cnmt");
    if (files.empty()) return fail("no .cnmt in the partition");
    const PfsEntry* e = files.front();
    return Bytes(pfs.begin() + (long)e->offset, pfs.begin() + (long)(e->offset + e->size));
}

}   // namespace install
