// install/compressed_storage.hpp: the compression layer's blocks and table.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/compressed_storage.hpp"

#include <algorithm>
#include <cstring>

namespace install::cstore
{

namespace
{
constexpr uint32_t MAGIC = 0x52544b42;   // "BKTR"
constexpr uint32_t VERSION = 1;

uint32_t sets_for(uint32_t entries) { return (entries + ENTRIES_PER_SET - 1) / ENTRIES_PER_SET; }

// A block must save more than this to be kept compressed.
uint32_t min_saving(size_t len) { return (uint32_t)std::max<size_t>(len / 256, 64); }

void put_node(uint8_t* node, int32_t index, int32_t count, int64_t end)
{
    put_le32(node, (uint32_t)index);
    put_le32(node + 4, (uint32_t)count);
    put_le64(node + 8, (uint64_t)end);
}
}   // namespace

std::array<uint8_t, HEADER_SIZE> table_header(int32_t entry_count)
{
    std::array<uint8_t, HEADER_SIZE> h{};
    put_le32(h.data(), MAGIC);
    put_le32(h.data() + 4, VERSION);
    put_le32(h.data() + 8, (uint32_t)entry_count);
    return h;
}

uint64_t table_size(uint32_t entry_count)
{
    return (uint64_t)NODE_SIZE * (1 + sets_for(entry_count));
}

// ---------------------------------------------------------------- Builder

Builder::Builder(uint32_t bs, CompressFn c, DecompressFn v, SinkFn s)
    : block_size(bs), compress(std::move(c)), verify(std::move(v)), sink(std::move(s))
{
    if (bs < MIN_BLOCK || bs > MAX_BLOCK || (bs & (bs - 1)) != 0) fail_with("a block size that is not a power of two from 4 to 64 KiB");
    pending.reserve(bs);
}

bool Builder::fail_with(std::string why)
{
    if (!failed) err = std::move(why);
    failed = true;
    return false;
}

bool Builder::emit(std::span<const uint8_t> b)
{
    if (b.empty()) return true;
    if (!sink(b)) return fail_with("the data could not be written");
    data_end += b.size();
    return true;
}

bool Builder::pad()
{
    static const uint8_t zeros[DATA_ALIGN] = {};
    const size_t         gap = (size_t)((DATA_ALIGN - data_end % DATA_ALIGN) % DATA_ALIGN);
    return emit({zeros, gap});
}

bool Builder::close_run()
{
    if (!in_run) return true;
    in_run = false;
    return pad();
}

bool Builder::add(std::span<const uint8_t> bytes)
{
    if (failed) return false;
    if (finished) return fail_with("bytes after the end");
    while (!bytes.empty()) {
        if (pending.empty() && bytes.size() >= block_size) {
            if (!block(bytes.first(block_size))) return false;
            bytes = bytes.subspan(block_size);
            continue;
        }
        const size_t take = std::min<size_t>(block_size - pending.size(), bytes.size());
        const size_t have = pending.size();
        pending.resize(have + take);
        if (take) std::memcpy(pending.data() + have, bytes.data(), take);
        bytes = bytes.subspan(take);
        if (pending.size() == block_size) {
            if (!block(pending)) return false;
            pending.clear();
        }
    }
    return true;
}

bool Builder::block(std::span<const uint8_t> b)
{
    const size_t len = b.size();
    bool         done = false;
    if (len > min_saving(len) + 1) {
        const size_t room = len - min_saving(len) - 1;
        packed.resize(room);
        if (auto n = compress(b, {packed.data(), room}); n && *n > 0 && *n <= room) {
            check.resize(len);
            if (!verify({packed.data(), *n}, check) || std::memcmp(check.data(), b.data(), len) != 0)
                return fail_with("a block that does not decompress to what it was made from");
            if (!close_run()) return false;
            if (entries.size() >= MAX_ENTRIES) return fail_with("more blocks than one table holds: use a larger block size");
            entries.push_back({(int64_t)virt_end, (int64_t)data_end, Kind::Lz4, (int32_t)*n});
            if (!emit({packed.data(), *n}) || !pad()) return false;
            done = true;
        }
    }
    if (!done) {
        const bool extend = in_run && virt_end - (uint64_t)entries.back().virt + len <= MAX_STORED_RUN;
        if (!extend) {
            if (!close_run()) return false;
            if (entries.size() >= MAX_ENTRIES) return fail_with("more blocks than one table holds: use a larger block size");
            entries.push_back({(int64_t)virt_end, (int64_t)data_end, Kind::Stored, -1});
            in_run = true;
        }
        if (!emit(b)) return false;
    }
    virt_end += len;
    return true;
}

bool Builder::finish()
{
    if (failed) return false;
    if (finished) return true;
    if (!pending.empty() && !block(pending)) return false;
    pending.clear();
    if (!close_run()) return false;
    if (entries.empty()) return fail_with("an empty RomFS");
    finished = true;

    const uint32_t n = (uint32_t)entries.size(), sets = sets_for(n);
    table_bytes.assign(table_size(n), 0);
    uint8_t* l1 = table_bytes.data();
    put_node(l1, 0, (int32_t)sets, (int64_t)virt_end);
    for (uint32_t s = 0; s < sets; s++) {
        const uint32_t first = s * ENTRIES_PER_SET, count = std::min(ENTRIES_PER_SET, n - first);
        const int64_t  end = first + count < n ? entries[first + count].virt : (int64_t)virt_end;
        put_le64(l1 + NODE_HEADER + 8 * s, (uint64_t)entries[first].virt);
        uint8_t* node = table_bytes.data() + (size_t)NODE_SIZE * (1 + s);
        put_node(node, (int32_t)s, (int32_t)count, end);
        for (uint32_t i = 0; i < count; i++) {
            uint8_t*     e = node + NODE_HEADER + (size_t)ENTRY_SIZE * i;
            const Entry& en = entries[first + i];
            put_le64(e, (uint64_t)en.virt);
            put_le64(e + 8, (uint64_t)en.phys);
            e[0x10] = (uint8_t)en.kind;
            put_le32(e + 0x14, (uint32_t)en.phys_size);
        }
    }
    return true;
}

// ----------------------------------------------------------------- Reader

Parsed<Reader> Reader::open(std::span<const uint8_t> table, int32_t entry_count, uint64_t data_size,
                            DecompressFn decompress)
{
    if (entry_count <= 0 || (uint32_t)entry_count > MAX_ENTRIES) return fail("a compression table with no entries or too many");
    const uint32_t n = (uint32_t)entry_count, sets = sets_for(n);
    if (table.size() < table_size(n)) return fail("a compression table smaller than its entries need");

    const uint8_t* l1 = table.data();
    const int32_t  l1_index = (int32_t)le32(l1), l1_count = (int32_t)le32(l1 + 4);
    const int64_t  end = (int64_t)le64(l1 + 8);
    if (l1_index != 0 || l1_count != (int32_t)sets || end <= 0) return fail("a damaged compression table header");

    Reader r;
    r.virt_size = (uint64_t)end;
    r.data_size = data_size;
    r.decompress = std::move(decompress);
    r.entries.reserve(n);

    for (uint32_t s = 0; s < sets; s++) {
        const uint8_t* node = table.data() + (size_t)NODE_SIZE * (1 + s);
        const uint32_t first = s * ENTRIES_PER_SET, count = std::min(ENTRIES_PER_SET, n - first);
        if ((int32_t)le32(node) != (int32_t)s || le32(node + 4) != count) return fail("a damaged compression entry set");
        for (uint32_t i = 0; i < count; i++) {
            const uint8_t* e = node + NODE_HEADER + (size_t)ENTRY_SIZE * i;
            Entry          en;
            en.virt = (int64_t)le64(e);
            en.phys = (int64_t)le64(e + 8);
            en.kind = (Kind)e[0x10];
            en.phys_size = (int32_t)le32(e + 0x14);
            r.entries.push_back(en);
        }
        if ((int64_t)le64(l1 + NODE_HEADER + 8 * s) != r.entries[first].virt) return fail("a compression table whose node offsets differ from its entries");
        const int64_t set_end = first + count < n ? -1 : end;
        if (set_end >= 0 && (int64_t)le64(node + 8) != set_end) return fail("a compression entry set that ends elsewhere");
    }
    for (uint32_t s = 0; s + 1 < sets; s++) {
        const uint8_t* node = table.data() + (size_t)NODE_SIZE * (1 + s);
        if ((int64_t)le64(node + 8) != r.entries[(size_t)(s + 1) * ENTRIES_PER_SET].virt) return fail("a compression entry set that ends elsewhere");
    }

    for (uint32_t i = 0; i < n; i++) {
        const Entry& en = r.entries[i];
        const int64_t next = i + 1 < n ? r.entries[i + 1].virt : end;
        if ((i == 0 && en.virt != 0) || en.virt < 0 || next <= en.virt || next > end) return fail("compression entries out of order");
        const uint64_t len = (uint64_t)(next - en.virt);
        if (en.phys < 0 || (uint64_t)en.phys > data_size) return fail("a compression entry outside its data");
        switch (en.kind) {
        case Kind::Stored:
            if ((uint64_t)en.phys + len > data_size) return fail("a stored compression entry outside its data");
            break;
        case Kind::Zeros:
            break;
        case Kind::Lz4:
            if (en.phys_size <= 0 || (uint32_t)en.phys_size > MAX_BLOCK || len > MAX_BLOCK) return fail("a compressed entry of a size the firmware does not read");
            if ((uint64_t)en.phys % DATA_ALIGN != 0 || (uint64_t)en.phys + (uint32_t)en.phys_size > data_size) return fail("a compressed entry misplaced in its data");
            break;
        default:
            return fail("a compression entry of a kind that is not known");
        }
    }
    return r;
}

bool Reader::read(uint64_t offset, std::span<uint8_t> out, const FetchFn& fetch) const
{
    if (offset > virt_size || out.size() > virt_size - offset) return false;
    auto it = std::upper_bound(entries.begin(), entries.end(), (int64_t)offset,
                               [](int64_t v, const Entry& e) { return v < e.virt; });
    std::vector<uint8_t> packed, plain;
    while (!out.empty()) {
        if (it == entries.begin()) return false;
        const Entry&   en = *(it - 1);
        const int64_t  next = it == entries.end() ? (int64_t)virt_size : it->virt;
        const uint64_t len = (uint64_t)(next - en.virt), within = offset - (uint64_t)en.virt;
        const size_t   take = (size_t)std::min<uint64_t>(out.size(), len - within);
        switch (en.kind) {
        case Kind::Stored:
            if (!fetch((uint64_t)en.phys + within, out.first(take))) return false;
            break;
        case Kind::Zeros:
            std::memset(out.data(), 0, take);
            break;
        case Kind::Lz4:
            packed.resize((size_t)en.phys_size);
            plain.resize((size_t)len);
            if (!fetch((uint64_t)en.phys, packed) || !decompress(packed, plain)) return false;
            std::memcpy(out.data(), plain.data() + within, take);
            break;
        }
        out = out.subspan(take);
        offset += take;
        ++it;
    }
    return true;
}

}   // namespace install::cstore
