// The install formats (source/install/): the crypto against known answers of
// another implementation (vectors.hpp, make_vectors.py), the archive, content
// meta and NCA parsers on archives built here, well-formed and broken.
// With PLAYSHELF_WRITE_SEEDS=<dir>, the well-formed inputs are also written
// there, as the fuzzers' seeds (tests/fuzz/install_*/corpus).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "install/aes.hpp"
#include "install/bytes.hpp"
#include "install/cnmt.hpp"
#include "install/nca.hpp"
#include "install/pfs.hpp"
#include "install/rsa.hpp"
#include "util/sha256.hpp"
#include "builders.hpp"
#include "vectors.hpp"

using namespace install;
using namespace test_install;

namespace
{
std::string sha_hex(const Bytes& b)
{
    Sha256 h;
    h.update(b.data(), b.size());
    return h.hex();
}

void write_seed(const char* name, const Bytes& b)
{
    const char* dir = std::getenv("PLAYSHELF_WRITE_SEEDS");
    if (!dir) return;
    std::ofstream(std::string(dir) + "/" + name, std::ios::binary).write((const char*)b.data(), (long)b.size());
}

// ---------------------------------------------------------------- crypto

void test_aes()
{
    // FIPS 197, appendix C.1.
    const uint8_t key[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const uint8_t plain[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    const uint8_t want[16] = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30, 0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};
    uint8_t       out[16], back[16];
    const Aes128  aes(key);
    aes.encrypt_block(plain, out);
    CHECK(std::memcmp(out, want, 16) == 0);
    aes.decrypt_block(out, back);
    CHECK(std::memcmp(back, plain, 16) == 0);

    uint8_t ecb[32];
    std::memcpy(ecb, ECB_PLAIN, 32);
    aes128_ecb_encrypt(ECB_KEY, ecb, 32);
    CHECK(std::memcmp(ecb, ECB_CIPHER, 32) == 0);
    aes128_ecb_decrypt(ECB_KEY, ecb, 32);
    CHECK(std::memcmp(ecb, ECB_PLAIN, 32) == 0);
}

void test_xts()
{
    const Bytes plain = stream(3 * 0x200, XTS_PLAIN_SEED);
    Bytes       data = plain;
    aes128_xts_nintendo(XTS_KEY, data.data(), data.size(), 1, 0x200, true);
    CHECK(sha_hex(data) == XTS_CIPHER_SHA256);
    aes128_xts_nintendo(XTS_KEY, data.data(), data.size(), 1, 0x200, false);
    CHECK(data == plain);
    // Another first sector number gives other bytes.
    Bytes other = plain;
    aes128_xts_nintendo(XTS_KEY, other.data(), other.size(), 0, 0x200, true);
    CHECK(sha_hex(other) != XTS_CIPHER_SHA256);
}

void test_ctr()
{
    const Bytes     plain = stream(0x300, CTR_PLAIN_SEED);
    const Aes128Ctr ctr(CTR_KEY, CTR_IV);
    Bytes           whole = plain;
    ctr.crypt(whole.data(), whole.size(), 0);
    CHECK(sha_hex(whole) == CTR_CIPHER_SHA256);   // the counter carried into the upper half on the way
    // In pieces that start inside a block: the same bytes.
    Bytes pieces = plain;
    ctr.crypt(pieces.data(), 0x23, 0);
    ctr.crypt(pieces.data() + 0x23, 0x1f1, 0x23);
    ctr.crypt(pieces.data() + 0x214, pieces.size() - 0x214, 0x214);
    CHECK(pieces == whole);
    ctr.crypt(pieces.data(), pieces.size(), 0);
    CHECK(pieces == plain);
}

void test_rsa()
{
    const Bytes msg = stream(0x200, RSA_MESSAGE_SEED);
    CHECK(rsa2048_pss_sha256_verify(msg.data(), msg.size(), RSA_SIGNATURE, RSA_MODULUS));

    Bytes changed = msg;
    changed[0x100] ^= 1;
    CHECK(!rsa2048_pss_sha256_verify(changed.data(), changed.size(), RSA_SIGNATURE, RSA_MODULUS));

    uint8_t sig[256];
    std::memcpy(sig, RSA_SIGNATURE, 256);
    sig[200] ^= 0x40;
    CHECK(!rsa2048_pss_sha256_verify(msg.data(), msg.size(), sig, RSA_MODULUS));
    // A "signature" not below the modulus, and a modulus that is not odd.
    CHECK(!rsa2048_pss_sha256_verify(msg.data(), msg.size(), RSA_MODULUS, RSA_MODULUS));
    uint8_t even[256];
    std::memcpy(even, RSA_MODULUS, 256);
    even[255] &= 0xfe;
    CHECK(!rsa2048_pss_sha256_verify(msg.data(), msg.size(), RSA_SIGNATURE, even));
    // Zero and one sign nothing.
    uint8_t small[256] = {};
    CHECK(!rsa2048_pss_sha256_verify(msg.data(), msg.size(), small, RSA_MODULUS));
    small[255] = 1;
    CHECK(!rsa2048_pss_sha256_verify(msg.data(), msg.size(), small, RSA_MODULUS));
}

// ---------------------------------------------------------------- archives

void test_pfs()
{
    const std::vector<File> files = {{"0123456789abcdef0123456789abcdef.nca", stream(100, "a")},
                                     {"fedcba9876543210fedcba9876543210.cnmt.nca", stream(40, "b")},
                                     {"01000000000100000000000000000000.tik", stream(0x2c0, "c")}};
    const Bytes nsp = archive(PfsKind::Pfs0, files);
    write_seed("pfs0", nsp);

    const auto hs = pfs_header_size(nsp);
    if (!CHECK(hs)) return;
    CHECK(hs->kind == PfsKind::Pfs0);
    std::string why;
    const auto  p = pfs_parse(nsp, nsp.size(), &why);
    if (!CHECK(p)) return;
    CHECK(p->entries.size() == 3);
    CHECK(p->header_size == hs->size);
    const PfsEntry* cnmt = p->find("fedcba9876543210fedcba9876543210.cnmt.nca");
    if (!CHECK(cnmt)) return;
    CHECK(cnmt->size == 40);
    CHECK(Bytes(nsp.begin() + (long)cnmt->offset, nsp.begin() + (long)(cnmt->offset + cnmt->size)) == files[1].data);
    CHECK(p->ending_with(".cnmt.nca").size() == 1);
    CHECK(p->ending_with(".nca").size() == 2);
    CHECK(p->ending_with(".tik").size() == 1);
    CHECK(!p->find("missing.nca"));

    // The header alone parses (the data is someone else's to read).
    CHECK(pfs_parse({nsp.data(), (size_t)hs->size}));
    // Cut short; past the archive's size.
    CHECK(!pfs_parse({nsp.data(), (size_t)hs->size - 1}));
    CHECK(!pfs_parse(nsp, nsp.size() - 1, &why));
    // Neither magic; too many entries; a header past MAX_HEADER.
    Bytes bad = nsp;
    bad[0] = 'X';
    CHECK(!pfs_header_size(bad));
    bad = nsp;
    put_le32(bad.data() + 4, MAX_ENTRIES + 1);
    CHECK(!pfs_header_size(bad));
    bad = nsp;
    put_le32(bad.data() + 8, (uint32_t)MAX_HEADER);
    CHECK(!pfs_header_size(bad));
    // A name past the table, a name without its end, an empty name.
    bad = nsp;
    put_le32(bad.data() + PFS_BASE_HEADER + 16, 0x1000);
    CHECK(!pfs_parse(bad));
    bad = nsp;
    const size_t table = PFS_BASE_HEADER + 3 * 0x18;
    for (size_t i = table; i < hs->size; i++)
        if (!bad[i]) bad[i] = 'x';
    CHECK(!pfs_parse(bad));
    bad = nsp;
    put_le32(bad.data() + PFS_BASE_HEADER + 16, le32(nsp.data() + PFS_BASE_HEADER + 0x18 + 16) - 1);
    CHECK(!pfs_parse(bad));
    // An offset that wraps around.
    bad = nsp;
    put_le64(bad.data() + PFS_BASE_HEADER, ~0ull - 4);
    CHECK(!pfs_parse(bad));

    // HFS0: the hashes kept; a hashed size past the file's refused.
    const Bytes hfs = archive(PfsKind::Hfs0, {{"secure", stream(0x300, "s")}, {"normal", {}}});
    write_seed("hfs0", hfs);
    const auto h = pfs_parse(hfs, hfs.size());
    if (!CHECK(h)) return;
    CHECK(h->kind == PfsKind::Hfs0);
    CHECK(h->entries[0].hashed_size == 0x200);
    Sha256 sh;
    sh.update(hfs.data() + h->entries[0].offset, 0x200);
    CHECK(sh.digest() == h->entries[0].hash);
    CHECK(h->entries[1].size == 0);
    bad = hfs;
    put_le32(bad.data() + PFS_BASE_HEADER + 20, 0x301);
    CHECK(!pfs_parse(bad));
}

void test_xci()
{
    Bytes card(0x200);
    put_le32(card.data() + 0x100, 0x44414548);
    put_le64(card.data() + 0x130, 0xf000);
    put_le64(card.data() + 0x138, 0x200);
    write_seed("xci", card);
    const auto r = xci_root(card, 0x10000);
    if (!CHECK(r)) return;
    CHECK(r->offset == 0xf000 && r->header_size == 0x200);
    CHECK(!xci_root(card, 0xf100));   // past the image
    CHECK(!xci_root({card.data(), 0x1ff}));
    Bytes bad = card;
    bad[0x100] = 0;
    CHECK(!xci_root(bad));
    bad = card;
    put_le64(bad.data() + 0x130, 0x100);
    CHECK(!xci_root(bad));
    bad = card;
    put_le64(bad.data() + 0x130, ~0ull - 0x10);
    CHECK(!xci_root(bad));
}

// ---------------------------------------------------------------- content meta

void test_cnmt()
{
    const Bytes app = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, some_contents());
    write_seed("cnmt_app", app);
    std::string why;
    const auto  m = cnmt_parse(app, &why);
    if (!CHECK(m)) return;
    CHECK(m->title_id == 0x0100000000010000ull);
    CHECK(m->type == MetaType::Application);
    CHECK(m->application_id() == 0x0100000000010000ull);
    CHECK(m->required_system_version() == 0x0c000000);
    CHECK(m->required_download_system_version == 0x1234);
    CHECK(m->contents.size() == 3);
    CHECK(m->contents[0].size == 0x123456789aull);
    CHECK(m->contents[0].hash[5] == 0xaa);
    CHECK(m->installable().size() == 2);   // not the delta fragment

    ContentInfo meta_content;
    meta_content.id = id_of(0x44);
    meta_content.size = 0x1800;
    meta_content.type = ContentType::Meta;
    const Bytes rec = cnmt_install_record(*m, meta_content);
    CHECK(rec.size() == 8 + 0x10 + 3 * 0x18);
    CHECK(le16(rec.data()) == 0x10);
    CHECK(le16(rec.data() + 2) == 3);   // the .cnmt.nca and two contents
    CHECK(le16(rec.data() + 4) == 0);
    CHECK(rec[6] == 0x2);
    CHECK(le32(rec.data() + 8 + 8) == 0x0c000000);
    const uint8_t* first = rec.data() + 8 + 0x10;
    CHECK(std::memcmp(first, meta_content.id.data(), 16) == 0);
    CHECK(first[22] == (uint8_t)ContentType::Meta);
    const uint8_t* program = first + 0x18;
    CHECK(std::memcmp(program, id_of(0x11).data(), 16) == 0);
    CHECK(le32(program + 16) == 0x3456789a && program[20] == 0x12);
    CHECK(program[22] == (uint8_t)ContentType::Program);
    const Bytes older = cnmt_install_record(*m, meta_content, true);
    CHECK(le32(older.data() + 8 + 8) == 0);

    // A patch keeps its extended data, after the infos.
    const Bytes extra = stream(0x50, "patch data");
    const Bytes patch = packaged_cnmt(MetaType::Patch, 0x0100000000010800ull, 0x10000, some_contents(), extra);
    write_seed("cnmt_patch", patch);
    const auto pm = cnmt_parse(patch);
    if (!CHECK(pm)) return;
    CHECK(pm->application_id() == 0x0100000000010000ull);
    CHECK(pm->extended_data == extra);
    const Bytes prec = cnmt_install_record(*pm, meta_content);
    CHECK(Bytes(prec.end() - 0x50, prec.end()) == extra);

    // An add-on names its application, requires no system version.
    const Bytes aoc = packaged_cnmt(MetaType::AddOnContent, 0x0100000000011001ull, 0, {some_contents()[1]});
    const auto  am = cnmt_parse(aoc);
    if (CHECK(am)) {
        CHECK(am->application_id() == 0x0100000000010000ull);
        CHECK(am->required_system_version() == 0);
    }

    // Cut anywhere, it is refused, never read past.
    for (size_t n = 0; n < app.size(); n++)
        CHECK(!cnmt_parse({app.data(), n}));
    for (size_t n = 0; n < patch.size(); n++)
        CHECK(!cnmt_parse({patch.data(), n}));
    // An extended data size past the end.
    Bytes bad = patch;
    put_le32(bad.data() + 0x20 + 12, 0x10000);
    CHECK(!cnmt_parse(bad));

    // Content ids as file names.
    ContentId id{};
    for (int i = 0; i < 16; i++)
        id[i] = (uint8_t)(i * 17);
    CHECK(content_id_hex(id) == "00112233445566778899aabbccddeeff");
    CHECK(content_id_from_hex("00112233445566778899AABBCCDDEEFF") == id);
    CHECK(!content_id_from_hex("00112233445566778899aabbccddeef"));
    CHECK(!content_id_from_hex("00112233445566778899aabbccddeefg"));
}

// ---------------------------------------------------------------- NCA

void test_nca()
{
    const Bytes  cnmt = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, some_contents());
    const Bytes  nca = cnmt_nca(cnmt);
    const NcaKeys keys = test_keys();
    write_seed("nca_cnmt", nca);

    uint8_t head[NCA_HEADER];
    std::memcpy(head, nca.data(), NCA_HEADER);
    nca_header_crypt(head, HEADER_KEY, false);
    std::string why;
    const auto  h = nca_parse_header(head, nca.size(), &why);
    if (!CHECK(h)) {
        fprintf(stderr, "  %s\n", why.c_str());
        return;
    }
    CHECK(h->content_type == 1);
    CHECK(h->key_generation == 3);
    CHECK(h->key_area_generation() == 2);
    CHECK(h->size == nca.size());
    CHECK(h->sections[0].present && !h->sections[1].present);
    CHECK(h->sections[0].start == NCA_FULL_HEADER);
    CHECK(h->sections[0].encryption == NcaEncryption::Ctr);
    CHECK(!h->has_rights_id());
    CHECK(!nca_parse_header(head, nca.size() + 0x200));   // its file says another size

    const auto read = nca_read_cnmt(nca, keys, &why);
    if (CHECK(read))
        CHECK(*read == cnmt);
    else
        fprintf(stderr, "  %s\n", why.c_str());

    // The wrong header key: no NCA3. A key generation the console lacks.
    NcaKeys wrong = keys;
    wrong.header_key[0] ^= 1;
    CHECK(!nca_read_cnmt(nca, wrong));
    const Bytes newer = cnmt_nca(cnmt, 9);
    CHECK(!nca_read_cnmt(newer, keys, &why));
    // A changed fs header no longer matches its hash.
    uint8_t changed[NCA_HEADER];
    std::memcpy(changed, head, NCA_HEADER);
    changed[0x400 + 0x48] ^= 1;
    CHECK(!nca_parse_header(changed, 0, &why));
    // A section past the content's end.
    std::memcpy(changed, head, NCA_HEADER);
    put_le32(changed + 0x244, (uint32_t)(nca.size() / 0x200 + 1));
    CHECK(!nca_parse_header(changed));
    // Cut anywhere: refused.
    for (size_t n = 0; n < nca.size(); n += 0x100)
        CHECK(!nca_read_cnmt({nca.data(), n}, keys));

    // Game card to download: once, and only for a game card one.
    Bytes card = cnmt_nca(cnmt, 3, 1);
    CHECK(nca_make_download(card.data(), HEADER_KEY));
    std::memcpy(head, card.data(), NCA_HEADER);
    nca_header_crypt(head, HEADER_KEY, false);
    CHECK(head[0x204] == 0);
    CHECK(nca_parse_header(head, card.size()));
    CHECK(!nca_make_download(card.data(), HEADER_KEY));
    Bytes download = nca;
    CHECK(!nca_make_download(download.data(), HEADER_KEY));
    CHECK(download == nca);

    // The signature: not Nintendo's on a header made here; another key
    // generation unknown; a header signed with the test key valid with it.
    std::memcpy(head, nca.data(), NCA_HEADER);
    nca_header_crypt(head, HEADER_KEY, false);
    CHECK(nca_check_signature(head) == NcaSignature::Invalid);
    head[0x221] = 1;
    CHECK(nca_check_signature(head) == NcaSignature::UnknownKey);
    uint8_t signed_head[NCA_HEADER] = {};
    std::memcpy(signed_head, RSA_SIGNATURE, 256);
    const Bytes region = stream(0x200, RSA_MESSAGE_SEED);
    std::memcpy(signed_head + 0x200, region.data(), 0x200);
    CHECK(nca_signature_valid_with(signed_head, RSA_MODULUS));
    signed_head[0x3ff] ^= 1;
    CHECK(!nca_signature_valid_with(signed_head, RSA_MODULUS));
}
}   // namespace

int main()
{
    test_aes();
    test_xts();
    test_ctr();
    test_rsa();
    test_pfs();
    test_xci();
    test_cnmt();
    test_nca();
    CHECK_DONE("install formats: crypto, PFS0/HFS0, XCI, CNMT and NCA checks passed");
}
