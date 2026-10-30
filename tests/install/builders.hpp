// Inputs the install suites build: a fixed pseudo-random stream, and PFS0
// or HFS0 archives of given files.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "install/aes.hpp"
#include "install/bytes.hpp"
#include "install/cnmt.hpp"
#include "install/nca.hpp"
#include "install/pfs.hpp"
#include "util/sha256.hpp"

namespace test_install
{
using Bytes = std::vector<uint8_t>;
using namespace install;

// make_vectors.py's stream(): SHA-256 of the seed and a big-endian counter.
inline Bytes stream(size_t n, const char* seed)
{
    Bytes out;
    for (uint32_t i = 0; out.size() < n; i++) {
        Sha256        h;
        const uint8_t c[4] = {(uint8_t)(i >> 24), (uint8_t)(i >> 16), (uint8_t)(i >> 8), (uint8_t)i};
        h.update(std::string_view(seed));
        h.update(c);
        const auto d = h.digest();
        out.insert(out.end(), d.begin(), d.end());
    }
    out.resize(n);
    return out;
}

struct File
{
    std::string name;
    Bytes       data;
};

// A PFS0 or HFS0 archive of `files`, the string table padded to `pad`.
inline Bytes archive(PfsKind kind, const std::vector<File>& files, size_t pad = 0x20)
{
    const size_t esize = kind == PfsKind::Pfs0 ? 0x18 : 0x40;
    std::string  names;
    for (const File& f : files)
        names += f.name + '\0';
    while (names.size() % pad)
        names += '\0';
    Bytes out(PFS_BASE_HEADER + files.size() * esize + names.size());
    put_le32(out.data(), kind == PfsKind::Pfs0 ? 0x30534650 : 0x30534648);
    put_le32(out.data() + 4, (uint32_t)files.size());
    put_le32(out.data() + 8, (uint32_t)names.size());
    uint64_t data_at = 0;
    uint32_t name_at = 0;
    for (size_t i = 0; i < files.size(); i++) {
        uint8_t* e = out.data() + PFS_BASE_HEADER + i * esize;
        put_le64(e, data_at);
        put_le64(e + 8, files[i].data.size());
        put_le32(e + 16, name_at);
        if (kind == PfsKind::Hfs0) {
            const uint32_t hashed = (uint32_t)std::min<size_t>(files[i].data.size(), 0x200);
            put_le32(e + 20, hashed);
            Sha256 h;
            h.update(std::span(files[i].data).first(hashed));
            const auto d = h.digest();
            std::memcpy(e + 32, d.data(), 32);
        }
        data_at += files[i].data.size();
        name_at += (uint32_t)files[i].name.size() + 1;
    }
    std::memcpy(out.data() + PFS_BASE_HEADER + files.size() * esize, names.data(), names.size());
    for (const File& f : files)
        out.insert(out.end(), f.data.begin(), f.data.end());
    return out;
}

inline ContentId id_of(uint8_t b)
{
    ContentId id;
    id.fill(b);
    return id;
}

// A packaged content meta: `type`, an extended header for it, the contents.
inline Bytes packaged_cnmt(MetaType type, uint64_t title, uint32_t version, const std::vector<ContentInfo>& contents,
                    const Bytes& extended_data = {})
{
    Bytes ext;
    if (type == MetaType::Application) {
        ext.resize(0x10);
        put_le64(ext.data(), title | 0x800);      // its patch
        put_le32(ext.data() + 8, 0x0c000000);     // required system version
    } else if (type == MetaType::Patch) {
        ext.resize(0x18);
        put_le64(ext.data(), title & ~0x800ull);  // its application
        put_le32(ext.data() + 8, 0x10000000);
        put_le32(ext.data() + 12, (uint32_t)extended_data.size());
    } else if (type == MetaType::AddOnContent) {
        ext.resize(0x10);
        put_le64(ext.data(), 0x0100000000010000ull);
    }
    Bytes out(0x20);
    put_le64(out.data(), title);
    put_le32(out.data() + 8, version);
    out[0xc] = (uint8_t)type;
    put_le16(out.data() + 0xe, (uint16_t)ext.size());
    put_le16(out.data() + 0x10, (uint16_t)contents.size());
    put_le16(out.data() + 0x12, 0);
    out[0x14] = 0x2;   // attributes
    put_le32(out.data() + 0x18, 0x1234);
    out.insert(out.end(), ext.begin(), ext.end());
    for (const ContentInfo& c : contents) {
        uint8_t p[0x38] = {};
        std::memcpy(p, c.hash.data(), 32);
        std::memcpy(p + 32, c.id.data(), 16);
        put_le32(p + 48, (uint32_t)c.size);
        p[52] = (uint8_t)(c.size >> 32);
        p[53] = c.attributes;
        p[54] = (uint8_t)c.type;
        p[55] = c.id_offset;
        out.insert(out.end(), p, p + 0x38);
    }
    out.insert(out.end(), extended_data.begin(), extended_data.end());
    out.resize(out.size() + 0x20);   // the digest
    return out;
}

inline std::vector<ContentInfo> some_contents()
{
    ContentInfo program;
    program.id = id_of(0x11);
    program.size = 0x123456789aull;   // 40 bits
    program.type = ContentType::Program;
    program.hash.fill(0xaa);
    ContentInfo control;
    control.id = id_of(0x22);
    control.size = 0x40000;
    control.type = ContentType::Control;
    ContentInfo delta;
    delta.id = id_of(0x33);
    delta.size = 0x1000;
    delta.type = ContentType::DeltaFragment;
    return {program, control, delta};
}

// Test keys: the console's own come from core/ (spl).
inline const uint8_t HEADER_KEY[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
inline const uint8_t KAEK[16] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
inline const uint8_t SECTION_KEY[16] = {0x5e, 0xc7, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d};

inline NcaKeys test_keys()
{
    NcaKeys k;
    std::memcpy(k.header_key.data(), HEADER_KEY, 32);
    k.unwrap = [](uint8_t generation, uint8_t index, const uint8_t in[16], uint8_t out[16]) {
        if (generation > 4 || index != 0) return false;
        std::memcpy(out, in, 16);
        aes128_ecb_decrypt(KAEK, {out, 16});
        return true;
    };
    return k;
}

// A whole .cnmt.nca holding `cnmt`, encrypted as a console would read it:
// its section in CTR under SECTION_KEY, the key area under KAEK, the header
// under HEADER_KEY. key_generation and the distribution as asked.
inline Bytes cnmt_nca(const Bytes& cnmt, uint8_t key_generation = 3, uint8_t distribution = 0)
{
    Bytes       pfs = archive(PfsKind::Pfs0, {{"Application_0100000000010000.cnmt", cnmt}});
    const size_t hash_table = 0x20;
    const size_t section_size = ((hash_table + pfs.size() + 0x1ff) / 0x200) * 0x200;
    Bytes        nca(NCA_FULL_HEADER + section_size);
    uint8_t*     h = nca.data();
    put_le32(h + 0x200, 0x3341434e);
    h[0x204] = distribution;
    h[0x205] = 1;   // meta
    h[0x206] = 2;
    h[0x220] = key_generation;
    h[0x207] = 0;
    put_le64(h + 0x208, nca.size());
    put_le64(h + 0x210, 0x0100000000010000ull);
    put_le32(h + 0x240, NCA_FULL_HEADER / 0x200);
    put_le32(h + 0x244, (uint32_t)(nca.size() / 0x200));
    uint8_t* fs = h + 0x400;
    put_le16(fs, 2);
    fs[2] = 1;   // PartitionFS
    fs[3] = 2;   // hierarchical SHA-256
    fs[4] = 3;   // CTR
    put_le64(fs + 0x40, hash_table);
    put_le64(fs + 0x48, pfs.size());
    put_le64(fs + 0x140, 0x0000000500000000ull);   // the upper counter
    Sha256 sh;
    sh.update({fs, 0x200});
    const auto d = sh.digest();
    std::memcpy(h + 0x280, d.data(), 32);
    uint8_t key_area[0x40] = {};
    std::memcpy(key_area + 0x20, SECTION_KEY, 16);
    aes128_ecb_encrypt(KAEK, key_area);
    std::memcpy(h + 0x300, key_area, 0x40);

    std::memcpy(nca.data() + NCA_FULL_HEADER + hash_table, pfs.data(), pfs.size());
    NcaSection s;
    s.ctr = le64(fs + 0x140);
    const auto iv = nca_section_iv(s);
    Aes128Ctr(SECTION_KEY, iv.data()).crypt(std::span(nca).subspan(NCA_FULL_HEADER, section_size), NCA_FULL_HEADER);
    nca_header_crypt(h, HEADER_KEY, true);
    return nca;
}

}   // namespace test_install
