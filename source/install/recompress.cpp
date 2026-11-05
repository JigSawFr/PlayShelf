// install/recompress.hpp: a Program NCA over again, its RomFS compressed.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/recompress.hpp"

#include <algorithm>
#include <cstring>

#include "install/aes.hpp"
#include "install/ivfc.hpp"
#include "util/sha256.hpp"

namespace install
{

namespace
{
constexpr size_t   PIECE = 1u << 20;
constexpr uint64_t SECTOR = NCA_MEDIA;
constexpr size_t   FS_COMPRESSION = 0x178;   // the compression info in a file system header

uint64_t up(uint64_t v, uint64_t to) { return (v + to - 1) / to * to; }

// A section's bytes decrypted, from the NCA it is in.
bool read_nca(const ReadAtFn& rd, const std::array<uint8_t, 16>& key, const NcaSection& s, uint64_t from, std::span<uint8_t> out)
{
    if (from > s.end - s.start || out.size() > s.end - s.start - from) return false;
    if (!rd(s.start + from, out)) return false;
    if (s.encryption == NcaEncryption::Ctr) {
        const auto iv = nca_section_iv(s);
        Aes128Ctr(key.data(), iv.data()).crypt(out, s.start + from);
    }
    return true;
}
}   // namespace

// What goes to the sink: bytes at the NCA position `pos`, encrypted as the
// section they belong to is, and hashed first when they are the integrity
// tree's data.
struct ProgramRecompressor::Stream
{
    explicit Stream(const cstore::SinkFn& s) : sink(s) {}
    const cstore::SinkFn& sink;
    uint64_t              pos = NCA_FULL_HEADER;
    const Aes128Ctr*      ctr = nullptr;
    ivfc::Builder*        tree = nullptr;
    std::vector<uint8_t>  scratch;

    bool put(std::span<const uint8_t> b, bool hashed)
    {
        if (b.empty()) return true;
        if (hashed && tree) tree->add(b);
        scratch.resize(b.size());
        std::memcpy(scratch.data(), b.data(), b.size());
        if (ctr) ctr->crypt(scratch, pos);
        if (!sink(scratch)) return false;
        pos += b.size();
        return true;
    }
    bool zeros(uint64_t n, bool hashed)
    {
        static const uint8_t z[4096] = {};
        while (n) {
            const size_t k = (size_t)std::min<uint64_t>(n, sizeof z);
            if (!put({z, k}, hashed)) return false;
            n -= k;
        }
        return true;
    }
};

ProgramRecompressor::ProgramRecompressor(ReadAtFn original, uint64_t original_size, NcaKeys k, cstore::CompressFn c,
                                         cstore::DecompressFn d, uint32_t bs)
    : orig_read(std::move(original)), orig_size(original_size), keys(std::move(k)), compress(std::move(c)),
      decompress(std::move(d)), block_size(bs)
{
}

void ProgramRecompressor::with_update(ReadAtFn update, uint64_t update_size)
{
    upd_read = std::move(update);
    upd_size = update_size;
    merging = true;
}

Done ProgramRecompressor::plan_update()
{
    if (upd_size < NCA_HEADER) return fail("the update is smaller than an NCA's header");
    std::array<uint8_t, NCA_HEADER> enc{};
    if (!upd_read(0, enc)) return fail("the update could not be read");
    nca_header_crypt(enc.data(), keys.header_key.data(), false);
    auto h = nca_parse_header(enc.data(), upd_size);
    if (!h) return fail("the update: " + h.error());
    upd = *h;
    if (upd.content_type != 0) return fail("the update's program is not a program NCA");
    if (upd.has_rights_id()) return fail("an update sold with a ticket (its title key is not handled)");
    if (upd.compressed()) return fail("an update already stored compressed");
    if ((upd.program_id & ~0x1fffull) != (orig.program_id & ~0x1fffull) || upd.program_id == orig.program_id)
        return fail("the update is not this game's");

    bool any_ctr = false;
    for (const NcaSection& s : upd.sections)
        any_ctr = any_ctr || (s.present && (s.encryption == NcaEncryption::Ctr || s.encryption == NcaEncryption::Bktr));
    if (any_ctr) {
        const auto k = nca_ctr_key(upd, keys);
        if (!k) return fail("no key for the update's sections (key generation newer than the console)");
        upd_key = *k;
    }
    uint64_t last = NCA_HEADER;
    for (int i = 0; i < 4; i++) {
        const NcaSection& us = upd.sections[i];
        if (!us.present) continue;
        if (us.start < last) return fail("the update's sections are out of order");
        last = us.end;
        if (us.sparse()) return fail("a sparse section in the update");
        Src& dst = src[i];
        dst = Src{};
        dst.present = true;
        dst.update = true;
        dst.meta = us;
        if (us.encryption == NcaEncryption::Bktr) {
            const NcaSection& bs = orig.sections[i];
            if (!bs.present || bs.fs_type != 0 || bs.hash_type != 3 || us.fs_type != 0 || us.hash_type != 3)
                return fail("the update patches a section the game does not have as a RomFS");
            const auto info = bktr::read_patch_info(us.fs_header.data(), us.end - us.start);
            if (!info) return fail("the update's patch: " + info.error());
            bktr::PatchSource ps;
            ps.nca = upd_read;
            ps.section_start = us.start;
            ps.section_size = us.end - us.start;
            ps.key = upd_key;
            ps.upper_iv = us.ctr;
            ps.encrypted = true;
            ps.info = *info;
            ps.base = [this, i](uint64_t off, std::span<uint8_t> out) { return read_nca(orig_read, key, orig.sections[i], off, out); };
            ps.base_size = bs.end - bs.start;
            auto patched = bktr::PatchedSection::open(std::move(ps));
            if (!patched) return fail("the update's patch: " + patched.error());
            dst.patch = std::make_unique<bktr::PatchedSection>(std::move(*patched));
            // What it becomes: a plain CTR section of the patched RomFS.
            dst.update = false;
            dst.meta.encryption = NcaEncryption::Ctr;
            dst.meta.fs_header[4] = 3;
            std::memset(dst.meta.fs_header.data() + bktr::PATCH_INFO_AT, 0, 0x40);
        } else if (us.encryption != NcaEncryption::Ctr && us.encryption != NcaEncryption::None) {
            return fail("a section of the update encrypted another way than CTR");
        }
    }
    expect_sdk = upd.sdk_version;
    return {};
}

Done ProgramRecompressor::inspect()
{
    if (orig_size < NCA_HEADER) return fail("a content smaller than an NCA's header");
    std::array<uint8_t, NCA_HEADER> enc{};
    if (!orig_read(0, enc)) return fail("the content could not be read");
    plain_header = enc;
    nca_header_crypt(plain_header.data(), keys.header_key.data(), false);
    auto h = nca_parse_header(plain_header.data(), orig_size);
    if (!h) return fail(h.error());
    orig = *h;
    if (orig.content_type != 0) return fail("not a program NCA");
    if (orig.has_rights_id()) return fail("a program NCA sold with a ticket (its title key is not handled)");
    if (orig.compressed()) return fail("a program NCA already stored compressed");

    uint64_t last = NCA_HEADER;   // the system's NCAs start their sections at 0x4000, packed homebrew's right after the header
    romfs = -1;
    first_section = ~0ull;
    for (int i = 0; i < 4; i++) {
        const NcaSection& s = orig.sections[i];
        if (!s.present) continue;
        if (s.start < last) return fail("sections out of order");
        first_section = std::min(first_section, s.start);
        last = s.end;
        if (s.sparse()) return fail("a sparse section");
        if (s.encryption != NcaEncryption::Ctr && s.encryption != NcaEncryption::None)
            return fail("a section encrypted another way than CTR");
        if (s.fs_type == 0 && s.hash_type == 3) {
            if (romfs >= 0) return fail("more than one RomFS section");
            romfs = i;
        }
    }
    if (romfs < 0) return fail("no RomFS section");

    bool any_ctr = false;
    for (const NcaSection& s : orig.sections)
        any_ctr = any_ctr || (s.present && s.encryption == NcaEncryption::Ctr);
    if (any_ctr) {
        const auto k = nca_ctr_key(orig, keys);
        if (!k) return fail("no key for the sections (key generation newer than the console)");
        key = *k;
    }

    // Where each section of the new program comes from: the game's, then the update's over them.
    for (int i = 0; i < 4; i++) {
        src[i] = Src{};
        if (!orig.sections[i].present) continue;
        src[i].present = true;
        src[i].meta = orig.sections[i];
    }
    expect_sdk = orig.sdk_version;
    if (merging) {
        if (auto d = plan_update(); !d) return d;
        romfs = -1;
        for (int i = 0; i < 4; i++) {
            if (!src[i].present || src[i].meta.fs_type != 0 || src[i].meta.hash_type != 3) continue;
            if (romfs >= 0) return fail("more than one RomFS section");
            romfs = i;
        }
        if (romfs < 0) return fail("no RomFS section");
    }
    const auto l = ivfc::read_fs_header(src[romfs].meta.fs_header.data());
    if (!l) return fail(l.error());
    if (l->size[5] == 0 || l->offset[5] > (1ull << 50) || l->size[5] > (1ull << 50)) return fail("a RomFS image outside its section");
    if (!src[romfs].patch) {
        const NcaSection& r = src[romfs].meta;
        if (l->offset[5] > r.end - r.start || l->size[5] > r.end - r.start - l->offset[5]) return fail("a RomFS image outside its section");
    } else if (l->offset[5] > src[romfs].patch->size() || l->size[5] > src[romfs].patch->size() - l->offset[5]) {
        return fail("a RomFS image outside the patched section");
    }
    if (block_size < cstore::MIN_BLOCK || block_size > cstore::MAX_BLOCK || (block_size & (block_size - 1)))
        return fail("a block size that is not a power of two from 4 to 64 KiB");
    inspected = true;
    return {};
}

bool ProgramRecompressor::read_src(int i, uint64_t from, std::span<uint8_t> out)
{
    const Src& s = src[i];
    if (s.patch) return s.patch->read(from, out);
    return s.update ? read_nca(upd_read, upd_key, s.meta, from, out) : read_nca(orig_read, key, s.meta, from, out);
}

Done ProgramRecompressor::check_source(const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel)
{
    if (!inspected) return fail("inspect() first");
    const auto layout = ivfc::read_fs_header(src[romfs].meta.fs_header.data());
    if (!layout) return fail(layout.error());
    uint64_t total = 0, done = 0;
    for (size_t k = 0; k < ivfc::LEVELS; k++) total += layout->size[k];
    bool stopped = false;
    const Done d = ivfc::verify(*layout, [&](uint64_t off, std::span<uint8_t> out) {
        if (cancel && cancel->load()) {
            stopped = true;
            return false;
        }
        done += out.size();
        if (progress) progress(std::min(done, total), total);
        return read_src(romfs, off, out);
    });
    if (stopped) return fail("cancelled");
    if (!d) return fail(merging ? "the update's RomFS, patched over the game's, does not match its integrity tree: " + d.error()
                                : "the game's RomFS does not match its integrity tree: " + d.error());
    return {};
}

Done ProgramRecompressor::copy_section(int i, Stream& out)
{
    const NcaSection& s = src[i].meta;
    const uint64_t    len = s.end - s.start;
    std::vector<uint8_t> buf(PIECE);
    for (uint64_t at = 0; at < len;) {
        if (cancel_flag && cancel_flag->load()) return fail("cancelled");
        const size_t n = (size_t)std::min<uint64_t>(PIECE, len - at);
        if (!read_src(i, at, {buf.data(), n})) return fail("the content could not be read");
        if (!out.put({buf.data(), n}, false)) return fail("the new content could not be written");
        at += n;
        read_done += n;
        if (progress_fn) progress_fn(read_done, read_total);
    }
    return {};
}

Done ProgramRecompressor::rebuild_romfs(int i, Stream& out, std::array<uint8_t, 0x200>& fs_header)
{
    const NcaSection& s = src[i].meta;
    const auto        l = ivfc::read_fs_header(s.fs_header.data());
    if (!l) return fail(l.error());
    const uint64_t image = l->size[5], image_at = l->offset[5];

    ivfc::Builder tree;
    out.tree = &tree;
    bool write_failed = false;
    cstore::Builder blocks(block_size, compress, decompress, [&](std::span<const uint8_t> b) {
        if (!out.put(b, true)) {
            write_failed = true;
            return false;
        }
        return true;
    });

    std::vector<uint8_t> buf(PIECE);
    for (uint64_t at = 0; at < image;) {
        if (cancel_flag && cancel_flag->load()) return fail("cancelled");
        const size_t n = (size_t)std::min<uint64_t>(PIECE, image - at);
        if (!read_src(i, image_at + at, {buf.data(), n})) return fail("the content could not be read");
        if (!blocks.add({buf.data(), n})) return fail(write_failed ? "the new content could not be written" : blocks.error());
        at += n;
        read_done += n;
        if (progress_fn) progress_fn(read_done, read_total);
    }
    if (!blocks.finish()) return fail(write_failed ? "the new content could not be written" : blocks.error());

    // The blocks, padded to the tree's block size; then the table.
    const uint64_t stored = blocks.data_size(), table_offset = up(stored, ivfc::BLOCK);
    if (!out.zeros(table_offset - stored, true)) return fail("the new content could not be written");
    const std::vector<uint8_t>& table = blocks.table();
    if (!out.put(table, true)) return fail("the new content could not be written");

    ivfc::Result result = tree.finish();
    out.tree = nullptr;
    if (!out.put(result.hash_levels, false)) return fail("the new content could not be written");
    const uint64_t section_len = table_offset + table.size() + result.hash_levels.size();
    if (!out.zeros(up(section_len, SECTOR) - section_len, false)) return fail("the new content could not be written");
    romfs_in = src[i].patch ? src[i].patch->size() : s.end - s.start;
    romfs_out = section_len;

    // Its file system header: the original's, with the tree and the layer.
    fs_header = s.fs_header;
    std::memset(fs_header.data() + 0x08, 0, 0xf8);   // the hash data, and the patch info (0x100) a merged RomFS no longer has
    std::memset(fs_header.data() + bktr::PATCH_INFO_AT, 0, 0x40);
    ivfc::write_fs_header(fs_header.data(), result.layout);
    std::memset(fs_header.data() + FS_COMPRESSION, 0, 0x28);
    put_le64(fs_header.data() + FS_COMPRESSION, table_offset);
    put_le64(fs_header.data() + FS_COMPRESSION + 8, table.size());
    const auto bktr = cstore::table_header((int32_t)blocks.entry_count());
    std::memcpy(fs_header.data() + FS_COMPRESSION + 0x10, bktr.data(), bktr.size());
    return {};
}

Done ProgramRecompressor::build(const cstore::SinkFn& body, const std::function<void(uint64_t, uint64_t)>& progress,
                                const std::atomic<bool>* cancel)
{
    if (!inspected) return fail("inspect() first");
    progress_fn = progress;
    cancel_flag = cancel;
    read_done = 0;
    read_total = 0;
    for (int i = 0; i < 4; i++)
        if (src[i].present)
            read_total += i == romfs ? ivfc::read_fs_header(src[i].meta.fs_header.data())->size[5] : src[i].meta.end - src[i].meta.start;

    Stream out(body);
    std::array<std::array<uint8_t, 0x200>, 4> fs_headers{};
    std::array<std::pair<uint64_t, uint64_t>, 4> places{};
    for (int i = 0; i < 4; i++) {
        const NcaSection& s = src[i].meta;
        if (!src[i].present) continue;
        places[i].first = out.pos;
        std::optional<Aes128Ctr> ctr;
        if (s.encryption == NcaEncryption::Ctr) {
            const auto iv = nca_section_iv(s);
            ctr.emplace(key.data(), iv.data());
        }
        out.ctr = ctr ? &*ctr : nullptr;
        fs_headers[i] = s.fs_header;
        const Done done = i == romfs ? rebuild_romfs(i, out, fs_headers[i]) : copy_section(i, out);
        out.ctr = nullptr;
        if (!done) return done;
        places[i].second = out.pos;
    }
    size = out.pos;

    // The header: the original's, with what changed.
    std::array<uint8_t, NCA_HEADER> plain = plain_header;
    put_le64(plain.data() + 0x208, size);
    plain[0x204] = 0;   // a download NCA
    put_le32(plain.data() + 0x21c, expect_sdk);
    for (int i = 0; i < 4; i++) {
        if (!src[i].present) continue;
        put_le32(plain.data() + 0x240 + 0x10 * i, (uint32_t)(places[i].first / SECTOR));
        put_le32(plain.data() + 0x244 + 0x10 * i, (uint32_t)(places[i].second / SECTOR));
        put_le32(plain.data() + 0x248 + 0x10 * i, 1);
        Sha256 sha;
        sha.update(fs_headers[i]);
        const auto d = sha.digest();
        std::memcpy(plain.data() + 0x280 + 0x20 * i, d.data(), 32);
        std::memcpy(plain.data() + 0x400 + 0x200 * i, fs_headers[i].data(), 0x200);
    }
    nca_header_crypt(plain.data(), keys.header_key.data(), true);

    head.assign(NCA_FULL_HEADER, 0);
    std::memcpy(head.data(), plain.data(), NCA_HEADER);
    if (first_section >= NCA_FULL_HEADER) {   // the original's padding, as it was (zeros where it held a section)
        if (!orig_read(NCA_HEADER, {head.data() + NCA_HEADER, NCA_FULL_HEADER - NCA_HEADER}))
            return fail("the content could not be read");
    }
    return {};
}

Done ProgramRecompressor::verify(const ReadAtFn& made, uint64_t made_size,
                                 const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel)
{
    if (!inspected) return fail("inspect() first");
    if (made_size < NCA_FULL_HEADER || made_size % SECTOR != 0) return fail("a new content of a size no NCA has");
    std::array<uint8_t, NCA_HEADER> made_head{};
    if (!made(0, made_head)) return fail("the new content could not be read");
    nca_header_crypt(made_head.data(), keys.header_key.data(), false);
    const auto nh = nca_parse_header(made_head.data(), made_size);
    if (!nh) return fail("the new content: " + nh.error());
    if (nh->content_type != 0 || nh->program_id != orig.program_id || nh->has_rights_id() || nh->distribution != 0)
        return fail("the new content's header differs from the original's");
    if (nh->key_area != orig.key_area || nh->key_generation != orig.key_generation || nh->key_area_index != orig.key_area_index ||
        nh->sdk_version != expect_sdk || nh->signature_key_generation != orig.signature_key_generation)
        return fail("the new content's keys or version differ from the original's");
    for (int i = 0; i < 4; i++)
        if (nh->sections[i].present != src[i].present) return fail("the new content has other sections");
    if (nh->sections[romfs].compressed() == false) return fail("the new RomFS is not stored compressed");
    for (int i = 0; i < 4; i++)
        if (i != romfs && nh->sections[i].compressed()) return fail("a section other than the RomFS is stored compressed");

    // What is read of the new NCA, decrypted.
    auto read_made = [&](int i, uint64_t from, std::span<uint8_t> out) {
        const NcaSection& s = nh->sections[i];
        if (from > s.end - s.start || out.size() > s.end - s.start - from) return false;
        if (!made(s.start + from, out)) return false;
        if (s.encryption == NcaEncryption::Ctr) {
            const auto iv = nca_section_iv(s);
            Aes128Ctr(key.data(), iv.data()).crypt(out, s.start + from);
        }
        return true;
    };
    auto stopped = [&] { return cancel && cancel->load(); };

    uint64_t total = 0, done = 0;
    for (int i = 0; i < 4; i++)
        if (src[i].present) total += i == romfs ? ivfc::read_fs_header(src[i].meta.fs_header.data())->size[5] : src[i].meta.end - src[i].meta.start;

    std::vector<uint8_t> a(PIECE), b(PIECE);
    for (int i = 0; i < 4; i++) {   // the sections carried over
        if (!src[i].present || i == romfs) continue;
        const uint64_t len = src[i].meta.end - src[i].meta.start;
        if (nh->sections[i].end - nh->sections[i].start != len) return fail("a carried-over section changed size");
        if (std::memcmp(src[i].meta.fs_header.data(), nh->sections[i].fs_header.data(), 0x200) != 0) return fail("a carried-over section's header changed");
        for (uint64_t at = 0; at < len;) {
            if (stopped()) return fail("cancelled");
            const size_t n = (size_t)std::min<uint64_t>(PIECE, len - at);
            if (!read_src(i, at, {a.data(), n}) || !read_made(i, at, {b.data(), n})) return fail("a content could not be read");
            if (std::memcmp(a.data(), b.data(), n) != 0) return fail("a carried-over section differs from the original");
            at += n;
            done += n;
            if (progress) progress(done, total);
        }
    }

    // The RomFS: the tree, the table, the image.
    const NcaSection& rs = nh->sections[romfs];
    const auto        layout = ivfc::read_fs_header(rs.fs_header.data());
    if (!layout) return fail("the new RomFS: " + layout.error());
    if (layout->offset[5] != 0) return fail("the new RomFS data does not start its section");
    for (size_t k = 0; k < ivfc::LEVELS; k++)
        if (layout->offset[k] + layout->size[k] > rs.end - rs.start) return fail("a level of the new RomFS outside its section");
    uint64_t verified = 0;
    const Done tree = ivfc::verify(*layout, [&](uint64_t off, std::span<uint8_t> out) {
        if (stopped()) return false;
        verified += out.size();
        return read_made(romfs, off, out);
    });
    if (stopped()) return fail("cancelled");
    if (!tree) return fail("the new RomFS's integrity tree: " + tree.error());

    const uint8_t* fs = rs.fs_header.data();
    const uint64_t table_offset = le64(fs + FS_COMPRESSION), table_size = le64(fs + FS_COMPRESSION + 8);
    if (le32(fs + FS_COMPRESSION + 0x10) != 0x52544b42 || le32(fs + FS_COMPRESSION + 0x14) > 1) return fail("the new RomFS's table header is not one");
    if (table_offset % ivfc::BLOCK != 0 || table_size == 0 || table_size > (256u << 20) || table_offset + table_size != layout->size[5])
        return fail("the new RomFS's table is not where its data ends");
    std::vector<uint8_t> table((size_t)table_size);
    if (!read_made(romfs, table_offset, table)) return fail("the table could not be read");
    const auto reader = cstore::Reader::open(table, (int32_t)le32(fs + FS_COMPRESSION + 0x18), table_offset, decompress);
    if (!reader) return fail("the new RomFS's table: " + reader.error());
    const auto old_layout = ivfc::read_fs_header(src[romfs].meta.fs_header.data());
    if (reader->virtual_size() != old_layout->size[5]) return fail("the new RomFS is not the original's size");

    const uint64_t image = old_layout->size[5];
    const cstore::Reader::FetchFn fetch = [&](uint64_t off, std::span<uint8_t> out) { return read_made(romfs, off, out); };
    for (uint64_t at = 0; at < image;) {
        if (stopped()) return fail("cancelled");
        const size_t n = (size_t)std::min<uint64_t>(PIECE, image - at);
        if (!read_src(romfs, old_layout->offset[5] + at, {a.data(), n})) return fail("the original could not be read");
        if (!reader->read(at, {b.data(), n}, fetch)) return fail("the new RomFS could not be read back");
        if (std::memcmp(a.data(), b.data(), n) != 0) return fail("the new RomFS differs from the original");
        at += n;
        done += n;
        if (progress) progress(done, total);
    }
    return {};
}

}   // namespace install
