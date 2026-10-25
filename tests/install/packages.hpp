// Packages the install suites install: an application (a program, a
// control, its content meta, a ticket and its certificate) as an NSP, an
// NSZ, an XCI or an XCZ, encrypted with the test keys (builders.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <zstd.h>

#include "builders.hpp"
#include "check.h"

namespace test_install
{
// ---------------------------------------------------------------- a title

// An NCA of `type` with one CTR section of `body` bytes.
inline Bytes content_nca(uint8_t type, size_t body, const char* seed, uint8_t distribution = 0)
{
    const size_t section = ((body + 0x1ff) / 0x200) * 0x200;
    Bytes        nca(NCA_FULL_HEADER + section);
    uint8_t*     h = nca.data();
    put_le32(h + 0x200, 0x3341434e);
    h[0x204] = distribution;
    h[0x205] = type;
    h[0x220] = 3;
    put_le64(h + 0x208, nca.size());
    put_le64(h + 0x210, 0x0100000000010000ull);
    put_le32(h + 0x240, NCA_FULL_HEADER / 0x200);
    put_le32(h + 0x244, (uint32_t)(nca.size() / 0x200));
    uint8_t* fs = h + 0x400;
    put_le16(fs, 2);
    fs[4] = 3;
    put_le64(fs + 0x140, 0x0000000900000000ull);
    Sha256 sh;
    sh.update(fs, 0x200);
    const auto d = sh.digest();
    std::memcpy(h + 0x280, d.data(), 32);
    const Bytes data = stream(section, seed);
    std::copy(data.begin(), data.end(), nca.begin() + NCA_FULL_HEADER);
    NcaSection s;
    s.ctr = 0x0000000900000000ull;
    const auto iv = nca_section_iv(s);
    Aes128Ctr(SECTION_KEY, iv.data()).crypt(nca.data() + NCA_FULL_HEADER, section, NCA_FULL_HEADER);
    nca_header_crypt(h, HEADER_KEY, true);
    return nca;
}

inline ContentInfo info_of(const Bytes& nca, ContentType type, uint8_t id_byte)
{
    ContentInfo c;
    c.id = id_of(id_byte);
    c.size = nca.size();
    c.type = type;
    Sha256 s;
    s.update(nca.data(), nca.size());
    c.hash = s.digest();
    return c;
}

// The .ncz of a plain NCA: its body stored after the header, as one zstd stream.
inline Bytes ncz_of(const Bytes& nca)
{
    uint8_t head[NCA_HEADER];
    std::memcpy(head, nca.data(), NCA_HEADER);
    nca_header_crypt(head, HEADER_KEY, false);
    const auto h = nca_parse_header(head);
    REQUIRE(h);
    const NcaSection& s = h->sections[0];
    Bytes             plain(nca.begin() + NCA_FULL_HEADER, nca.end());
    const auto        iv = nca_section_iv(s);
    Aes128Ctr(SECTION_KEY, iv.data()).crypt(plain.data(), plain.size(), NCA_FULL_HEADER);
    Bytes out(nca.begin(), nca.begin() + NCA_FULL_HEADER);
    Bytes sec(16 + 0x40);
    put_le64(sec.data(), 0x4e544345535a434eull);
    put_le64(sec.data() + 8, 1);
    put_le64(sec.data() + 16, s.start);
    put_le64(sec.data() + 24, s.end - s.start);
    sec[32] = 3;
    std::memcpy(sec.data() + 16 + 0x20, SECTION_KEY, 16);
    std::memcpy(sec.data() + 16 + 0x30, iv.data(), 16);
    out.insert(out.end(), sec.begin(), sec.end());
    Bytes z(ZSTD_compressBound(plain.size()));
    z.resize(ZSTD_compress(z.data(), z.size(), plain.data(), plain.size(), 3));
    out.insert(out.end(), z.begin(), z.end());
    return out;
}

struct Title
{
    Bytes                program, control, cnmt, meta_nca;
    ContentInfo          program_info, control_info;
    std::vector<File>    files;   // the package's
    ContentId            meta_id = id_of(0x44);
};

// An application: a program and a control NCA, its content meta, a ticket
// and its certificate. compressed: the program as a .ncz.
inline Title application(bool compressed = false, uint32_t required_version = 0x0c000000)
{
    Title t;
    t.program = content_nca(0, 0x5300, "program");
    t.control = content_nca(2, 0x900, "control");
    t.program_info = info_of(t.program, ContentType::Program, 0x11);
    t.control_info = info_of(t.control, ContentType::Control, 0x22);
    t.cnmt = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, {t.program_info, t.control_info});
    put_le32(t.cnmt.data() + 0x20 + 8, required_version);
    t.meta_nca = cnmt_nca(t.cnmt);
    const std::string p = content_id_hex(t.program_info.id), c = content_id_hex(t.control_info.id);
    t.files = {{content_id_hex(t.meta_id) + ".cnmt.nca", t.meta_nca},
               compressed ? File{p + ".ncz", ncz_of(t.program)} : File{p + ".nca", t.program},
               {c + ".nca", t.control},
               {"01000000000100000000000000000003.tik", stream(0x2c0, "ticket")},
               {"01000000000100000000000000000003.cert", stream(0x700, "cert")}};
    return t;
}

inline Bytes nsp(const Title& t) { return archive(PfsKind::Pfs0, t.files); }

// A game card image of the title: a header, the root HFS0 with an empty
// "update" and the "secure" partition holding the files.
inline Bytes xci(const Title& t)
{
    const Bytes secure = archive(PfsKind::Hfs0, t.files, 0x200);
    const Bytes root = archive(PfsKind::Hfs0, {{"update", {}}, {"secure", secure}}, 0x200);
    Bytes       img(0xf000);
    put_le32(img.data() + 0x100, 0x44414548);
    put_le64(img.data() + 0x130, img.size());
    put_le64(img.data() + 0x138, pfs_header_size(root.data(), root.size())->size);
    img.insert(img.end(), root.begin(), root.end());
    return img;
}

}   // namespace test_install
