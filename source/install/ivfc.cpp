// install/ivfc.hpp: the hash tree of a RomFS section.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/ivfc.hpp"

#include <algorithm>
#include <cstring>

namespace install::ivfc
{

namespace
{
constexpr uint32_t MAGIC = 0x43465649;   // "IVFC"
constexpr uint32_t VERSION = 0x20000;
constexpr uint32_t MAX_LAYERS = 7;
constexpr uint32_t MIN_ORDER = 9, MAX_ORDER = 24;   // block sizes from 512 bytes to 16 MiB

// A level of `size` bytes padded: up to the next multiple of BLOCK, a whole
// extra block when it already is one.
uint64_t padded(uint64_t size) { return size + (BLOCK - size % BLOCK); }

std::array<uint8_t, HASH> hash_of(std::span<const uint8_t> b)
{
    Sha256 h;
    h.update(b);
    return h.digest();
}
}   // namespace

void Builder::block(std::span<const uint8_t> b)
{
    const auto d = hash_of(b);
    hashes.insert(hashes.end(), d.begin(), d.end());
}

void Builder::add(std::span<const uint8_t> data)
{
    total += data.size();
    if (!partial.empty()) {
        const size_t take = std::min<size_t>(BLOCK - partial.size(), data.size());
        const size_t have = partial.size();
        partial.resize(have + take);
        if (take) std::memcpy(partial.data() + have, data.data(), take);
        data = data.subspan(take);
        if (partial.size() < BLOCK) return;
        block(partial);
        partial.clear();
    }
    while (data.size() >= BLOCK) {
        block(data.first(BLOCK));
        data = data.subspan(BLOCK);
    }
    partial.resize(data.size());
    if (!data.empty()) std::memcpy(partial.data(), data.data(), data.size());
}

Result Builder::finish()
{
    if (!partial.empty()) {
        partial.resize(BLOCK, 0);
        block(partial);
        partial.clear();
    }
    // levels[4] hashes the data; levels[i] hashes levels[i + 1].
    std::array<std::vector<uint8_t>, 5> levels;
    levels[4] = std::move(hashes);
    levels[4].resize(padded(levels[4].size()), 0);
    for (int i = 3; i >= 0; i--) {
        std::vector<uint8_t> up;
        for (size_t at = 0; at < levels[i + 1].size(); at += BLOCK) {
            const auto d = hash_of({levels[i + 1].data() + at, BLOCK});
            up.insert(up.end(), d.begin(), d.end());
        }
        up.resize(padded(up.size()), 0);
        levels[i] = std::move(up);
    }

    Result r;
    uint64_t at = total;   // the hash levels follow the data
    for (size_t i = 0; i < 5; i++) {
        r.layout.offset[i] = at;
        r.layout.size[i] = levels[i].size();
        at += levels[i].size();
        r.hash_levels.insert(r.hash_levels.end(), levels[i].begin(), levels[i].end());
    }
    r.layout.offset[5] = 0;
    r.layout.size[5] = total;
    r.layout.order.fill(BLOCK_ORDER);
    r.layout.master = hash_of(levels[0]);
    return r;
}

uint64_t hash_levels_size(uint64_t data_size)
{
    uint64_t total = 0, level = data_size;
    for (int i = 0; i < 5; i++) {
        const uint64_t hashes = (level + BLOCK - 1) / BLOCK * HASH;
        level = padded(hashes);
        total += level;
    }
    return total;
}

void write_fs_header(uint8_t* fs, const Layout& l)
{
    put_le32(fs + 0x08, MAGIC);
    put_le32(fs + 0x0c, VERSION);
    put_le32(fs + 0x10, (uint32_t)HASH);
    put_le32(fs + 0x14, MAX_LAYERS);
    for (size_t i = 0; i < LEVELS; i++) {
        uint8_t* e = fs + 0x18 + 0x18 * i;
        put_le64(e, l.offset[i]);
        put_le64(e + 8, l.size[i]);
        put_le32(e + 0x10, l.order[i]);
        put_le32(e + 0x14, 0);
    }
    std::memcpy(fs + 0xc8, l.master.data(), HASH);
}

Parsed<Layout> read_fs_header(const uint8_t* fs)
{
    if (le32(fs + 0x08) != MAGIC || le32(fs + 0x0c) != VERSION) return fail("not an IVFC header");
    if (le32(fs + 0x10) != HASH || le32(fs + 0x14) != MAX_LAYERS) return fail("an IVFC header of another shape");
    Layout l;
    for (size_t i = 0; i < LEVELS; i++) {
        const uint8_t* e = fs + 0x18 + 0x18 * i;
        l.offset[i] = le64(e);
        l.size[i] = le64(e + 8);
        l.order[i] = le32(e + 0x10);
        if (l.order[i] < MIN_ORDER || l.order[i] > MAX_ORDER) return fail("an IVFC block size that is not supported");
        if (l.offset[i] > (1ull << 50) || l.size[i] > (1ull << 50)) return fail("an IVFC level out of range");
    }
    std::memcpy(l.master.data(), fs + 0xc8, HASH);
    return l;
}

Done verify(const Layout& l, const ReadFn& read)
{
    std::vector<uint8_t> top(l.size[0]);
    if (l.size[0] == 0 || l.size[0] > (64u << 20) || !read(l.offset[0], top)) return fail("the top hash level could not be read");
    if (hash_of(top) != l.master) return fail("the master hash does not match the top hash level");

    std::vector<uint8_t> block, want;
    for (size_t i = 0; i < 5; i++) {
        if (l.order[i + 1] < MIN_ORDER || l.order[i + 1] > MAX_ORDER) return fail("an IVFC block size that is not supported");
        const size_t   bs = (size_t)1 << l.order[i + 1];
        const uint64_t n = (l.size[i + 1] + bs - 1) / bs;
        if (n * HASH > l.size[i]) return fail("a hash level too small for the level it covers");
        for (uint64_t j = 0; j < n; j++) {
            const uint64_t at = j * bs, have = std::min<uint64_t>(bs, l.size[i + 1] - at);
            block.assign(bs, 0);   // the last block, short, is hashed as if padded with zeros
            if (!read(l.offset[i + 1] + at, {block.data(), (size_t)have})) return fail("a level could not be read");
            want.resize(HASH);
            if (!read(l.offset[i] + j * HASH, want)) return fail("a hash could not be read");
            const auto got = hash_of(block);
            if (std::memcmp(got.data(), want.data(), HASH) != 0) return fail("a block whose hash does not match");
        }
    }
    return {};
}

}   // namespace install::ivfc
