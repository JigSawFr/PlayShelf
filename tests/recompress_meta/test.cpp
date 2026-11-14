// install/recompress_meta.cpp: a content meta with its program replaced
// (id, size, hash, the system version raised, the rest untouched, the same
// length) and a .cnmt.nca built here (hash table, master hash, CTR) with that
// content meta put in: the NCA keeps its size and layout, its content meta
// reads back as the new one, the hash table and every hash above it match what
// is computed from scratch here. And what is refused.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#include "../install/recompress_builders.hpp"
#include "check.h"
#include "install/aes.hpp"
#include "install/recompress_meta.hpp"

using namespace install;
using namespace test_install;

namespace
{
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
    // Another length: the NCA is made again, the same as one built around the new content meta.
    {
        auto more = contents();
        ContentInfo extra;
        extra.id = id_of(0x66);
        extra.size = 1234;
        extra.type = ContentType::Data;
        more.push_back(extra);
        more.push_back(extra);
        const Bytes longer = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, more);
        const Bytes shorter = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, {contents()[0]});
        for (const Bytes* c : {&longer, &shorter}) {
            for (uint32_t block : {0x200u, 0x1000u}) {
                const Bytes nca = meta_nca(cnmt, block);
                auto        out = meta_nca_with_cnmt(nca, *c, test_keys());
                if (!CHECK(out.has_value())) continue;
                CHECK(*out == meta_nca(*c, block));
                auto read = nca_read_cnmt(*out, test_keys());
                if (CHECK(read.has_value())) CHECK(*read == *c);
            }
        }
        // A section other than the first: not a content meta NCA this can rebuild.
        Bytes two = meta_nca(cnmt);
        nca_header_crypt(two.data(), HEADER_KEY, false);
        put_le32(two.data() + 0x250, 0x50);
        put_le32(two.data() + 0x254, 0x60);
        nca_header_crypt(two.data(), HEADER_KEY, true);
        CHECK(!meta_nca_with_cnmt(two, longer, test_keys()).has_value());
    }
    // A game card one stays as it is (the distribution is not this module's).
    {
        auto out = meta_nca_with_cnmt(meta_nca(cnmt, 0x1000, 1), patched, test_keys());
        CHECK(out.has_value());
    }
    // Refusals.
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
void test_record()
{
    ContentInfo meta_content;
    meta_content.id = id_of(0x44);
    meta_content.size = 0x8000;
    meta_content.type = ContentType::Meta;
    const auto packaged = packaged_cnmt(MetaType::Application, 0x0100000000010000ull, 0, contents());
    const auto meta = cnmt_parse(packaged);
    if (!CHECK(meta.has_value())) return;
    const auto record = cnmt_install_record(*meta, meta_content);

    auto infos = record_contents(record);
    if (!CHECK(infos.has_value())) return;
    CHECK(infos->size() == 3);
    CHECK((*infos)[0].id == id_of(0x44) && (*infos)[0].type == ContentType::Meta && (*infos)[0].size == 0x8000);
    CHECK((*infos)[1].id == id_of(0x11) && (*infos)[1].type == ContentType::Program && (*infos)[1].size == 0x1234567);
    CHECK((*infos)[2].id == id_of(0x22) && (*infos)[2].type == ContentType::Control);

    auto out = record_with_contents(record, MetaType::Application, {{id_of(0x11), id_of(0x99), 0x2'0000'1000ull}, {id_of(0x44), id_of(0x98), 0x8000}});
    if (!CHECK(out.has_value())) return;
    CHECK(out->size() == record.size());
    auto now_infos = record_contents(*out);
    if (!CHECK(now_infos.has_value())) return;
    CHECK((*now_infos)[0].id == id_of(0x98) && (*now_infos)[0].type == ContentType::Meta);
    CHECK((*now_infos)[1].id == id_of(0x99) && (*now_infos)[1].size == 0x2'0000'1000ull && (*now_infos)[1].type == ContentType::Program);
    CHECK((*now_infos)[2] .id == id_of(0x22));
    CHECK(le32(out->data() + 8 + 8) == SYSTEM_VERSION_12_0_0);
    // The record the new content meta would give is the same bytes (the infos there are the new ones).
    {
        ContentInfo np;
        np = now();
        ContentInfo nm = meta_content;
        nm.id = id_of(0x98);
        auto patched = cnmt_with_program(packaged, id_of(0x11), np);
        auto pm = cnmt_parse(*patched);
        if (CHECK(pm.has_value())) CHECK(cnmt_install_record(*pm, nm) == record_with_contents(record, MetaType::Application, {{id_of(0x11), np.id, np.size}, {id_of(0x44), nm.id, nm.size}}).value());
    }
    CHECK(!record_with_contents(record, MetaType::Application, {{id_of(0x77), id_of(0x99), 1}}).has_value());
    CHECK(!record_contents(std::span<const uint8_t>(record.data(), 5)).has_value());
    CHECK(!record_contents(std::span<const uint8_t>(record.data(), 20)).has_value());
    // Other types keep their version.
    auto add = record_with_contents(record, MetaType::AddOnContent, {{id_of(0x11), id_of(0x99), 1}});
    if (CHECK(add.has_value())) CHECK(le32(add->data() + 16) == le32(record.data() + 16));
}

// The game with its update merged in: whole (not Compacted, no application version required), at the update's
// version, asking the firmware the two ask but no more than the console's records accept, and 12.0.0 at least.
void test_merged_cnmt()
{
    const uint64_t app = 0x0100000000010000ull;
    auto           base = cnmt_parse(packaged_cnmt(MetaType::Application, app, 0, contents()));
    auto           patch = cnmt_parse(packaged_cnmt(MetaType::Patch, app | 0x800, 65536, contents(), Bytes(0)));
    if (!CHECK(base.has_value() && patch.has_value())) return;
    base->attributes = 0x01 | CNMT_ATTRIBUTE_COMPACTED;   // another attribute stays
    put_le32(base->extended_header.data() + 12, 7);       // an application version required
    put_le32(patch->extended_header.data() + 8, 0x40000000);   // the update asks for 16.0.0

    const auto merged = [&](RecordedSystem r) { return merged_cnmt(*base, *patch, now(), r); };
    const auto asks = [](const ContentMeta& m) { return le32(m.extended_header.data() + 8); };

    ContentMeta m = merged({});
    CHECK(m.title_id == app && m.type == MetaType::Application && m.version == 65536);
    CHECK(m.attributes == 0x01);   // no longer Compacted
    CHECK(le32(m.extended_header.data() + 12) == 0);
    CHECK(m.contents.size() >= 1 && m.contents[0].id == now().id && m.contents[0].type == ContentType::Program);
    CHECK(asks(m) == 0x40000000);   // nothing known of the records: what the two ask

    CHECK(asks(merged({0x30000000, 0x30000000})) == SYSTEM_VERSION_12_0_0);   // the records accept 12.0.0: no more
    CHECK(asks(merged({0x30000000, 0x40000000})) == 0x40000000);              // the update's record says 16.0.0
    CHECK(asks(merged({0, 0})) == SYSTEM_VERSION_12_0_0);                      // lowered to nothing: 12.0.0 still
    CHECK(asks(merged({0x50000000, 0x50000000})) == 0x40000000);              // never above what the metas ask
    CHECK(asks(merged({0x30000000, UINT32_MAX})) == 0x40000000);               // one record unknown: what the metas ask
    put_le32(patch->extended_header.data() + 8, 0x10000000);                   // both ask below 12.0.0
    CHECK(asks(merged({})) == SYSTEM_VERSION_12_0_0);

    // The version a record holds.
    ContentInfo meta_content;
    meta_content.id = id_of(0x33);
    meta_content.type = ContentType::Meta;
    Bytes record = cnmt_install_record(*base, meta_content);
    CHECK(recorded_system_version(record) == le32(base->extended_header.data() + 8));
    put_le32(record.data() + 16, 0x2a000000);
    CHECK(recorded_system_version(record) == 0x2a000000);
    CHECK(recorded_system_version(std::span<const uint8_t>(record.data(), 7)) == UINT32_MAX);
    CHECK(recorded_system_version(std::span<const uint8_t>(record.data(), 12)) == UINT32_MAX);   // cut inside the header
    Bytes short_ext = record;
    short_ext[0] = 8;   // an extended header too short to hold a version
    short_ext[1] = 0;
    CHECK(recorded_system_version(short_ext) == UINT32_MAX);
}
}   // namespace

int main()
{
    test_content_ids();
    test_cnmt();
    test_meta_nca();
    test_record();
    test_merged_cnmt();
    CHECK_DONE("recompress_meta: ok");
}
