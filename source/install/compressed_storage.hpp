// compressed_storage — the firmware's compression layer of an NCA section
// (12.0.0 and later): a RomFS cut in blocks, each one LZ4-compressed or
// stored, and a bucket tree ("BKTR") that says where each block's bytes are.
// The section reads, from the outside in: AES-CTR, the IVFC hash levels, this
// layer, then the RomFS the game sees. The IVFC hashes cover what this layer
// writes (the compressed blocks and the table), not the RomFS.
//
// What the layer holds, in the order of the IVFC data level: the blocks, each
// at a multiple of 16 bytes; then, at the table offset the FS header names,
// the table: one 16 KiB node (the offsets of the entry sets), then one 16 KiB
// entry set per 681 entries. An entry is 0x18 bytes: the RomFS offset it
// starts at (s64), where its bytes are in the data (s64), how they are kept
// (u8: 0 stored, 1 zeros, 3 LZ4), and how many they are (s32; unused for
// stored). The layout is written from the format's description, and the rules
// the reader enforces are the firmware's as its source shows them: ordered
// entries, bytes inside the data, a compressed entry of at most 64 KiB read
// whole. Only the single-node tree is built and read (up to 2046 entry sets:
// 64 KiB blocks reach 87 GB); a bigger one is refused rather than guessed.
//
// This is self-consistency, not the firmware's word: a table that reads back
// here is not proof the console accepts it.
//
// The codec is the caller's (liblz4 on a console and in the tests), so the
// layout code takes no dependency.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "install/bytes.hpp"

namespace install::cstore
{

constexpr uint32_t NODE_SIZE = 0x4000;
constexpr uint32_t NODE_HEADER = 0x10;
constexpr uint32_t ENTRY_SIZE = 0x18;
constexpr uint32_t ENTRIES_PER_SET = (NODE_SIZE - NODE_HEADER) / ENTRY_SIZE;   // 681
constexpr uint32_t OFFSETS_PER_NODE = (NODE_SIZE - NODE_HEADER) / 8;           // 2046
constexpr uint32_t MAX_ENTRIES = ENTRIES_PER_SET * OFFSETS_PER_NODE;
constexpr uint32_t MAX_BLOCK = 64 * 1024;       // a compressed entry's reach, at most
constexpr uint32_t MIN_BLOCK = 4 * 1024;
constexpr uint32_t DATA_ALIGN = 16;             // where a compressed block may start
constexpr uint64_t MAX_STORED_RUN = 64ull << 20;   // stored blocks merged into one entry
constexpr uint32_t HEADER_SIZE = 0x10;          // the BKTR header kept in the FS header

enum class Kind : uint8_t { Stored = 0, Zeros = 1, Lz4 = 3 };

struct Entry
{
    int64_t  virt = 0;        // the RomFS offset this entry starts at
    int64_t  phys = 0;        // where its bytes are, in the data
    Kind     kind = Kind::Stored;
    int32_t  phys_size = 0;   // its bytes in the data (-1 for a stored run)
};

// Compresses `in` into `out` (room for out.size() bytes): the bytes made, or
// nothing when it does not fit or does not shrink.
using CompressFn = std::function<std::optional<size_t>(std::span<const uint8_t> in, std::span<uint8_t> out)>;
// Decompresses `in` into `out`, which it must fill exactly.
using DecompressFn = std::function<bool(std::span<const uint8_t> in, std::span<uint8_t> out)>;
// Takes the next bytes of the data, in order; false stops the build.
using SinkFn = std::function<bool(std::span<const uint8_t> bytes)>;

// The 0x10 bytes of the FS header (at compression_info + 0x10) for a table.
std::array<uint8_t, HEADER_SIZE> table_header(int32_t entry_count);

// The size of the table a count of entries takes (node + entry sets).
uint64_t table_size(uint32_t entry_count);

// Cuts a RomFS, fed in pieces of any size, into blocks; each is compressed
// when that saves enough, read back and compared before it is kept, else
// stored. The data goes to the sink as it is made; finish() makes the table.
class Builder
{
  public:
    // block_size: a power of two from MIN_BLOCK to MAX_BLOCK.
    Builder(uint32_t block_size, CompressFn compress, DecompressFn verify, SinkFn sink);

    bool add(std::span<const uint8_t> bytes);
    // The last block, then the table. False once anything failed: error().
    bool finish();

    const std::string&          error() const { return err; }
    uint64_t                    data_size() const { return data_end; }   // bytes sent to the sink, padding included
    uint64_t                    virtual_size() const { return virt_end; }
    uint32_t                    entry_count() const { return (uint32_t)entries.size(); }
    // After finish(): the table, to place at the table offset.
    const std::vector<uint8_t>& table() const { return table_bytes; }

  private:
    bool block(std::span<const uint8_t> b);
    bool emit(std::span<const uint8_t> b);
    bool close_run();
    bool pad();
    bool fail_with(std::string why);

    uint32_t                block_size;
    CompressFn              compress;
    DecompressFn            verify;
    SinkFn                  sink;
    std::vector<uint8_t>    pending, packed, check;
    std::vector<Entry>      entries;
    std::vector<uint8_t>    table_bytes;
    uint64_t                data_end = 0, virt_end = 0;
    bool                    in_run = false, failed = false, finished = false;
    std::string             err;
};

// A table checked against the rules above, then read from: `data_size` is the
// size of the data it points into (the table offset), `virtual_size` the
// RomFS's (none known: pass what the table's own end says, 0 to take it).
class Reader
{
  public:
    static Parsed<Reader> open(std::span<const uint8_t> table, int32_t entry_count, uint64_t data_size,
                               DecompressFn decompress);

    uint64_t                   virtual_size() const { return virt_size; }
    const std::vector<Entry>&  all() const { return entries; }

    // Reads the RomFS bytes at [offset, offset+out.size()); `fetch` gives the
    // data's bytes at an offset. False: the range is past the end, a fetch
    // failed, or a block did not decompress to its size.
    using FetchFn = std::function<bool(uint64_t offset, std::span<uint8_t> out)>;
    bool read(uint64_t offset, std::span<uint8_t> out, const FetchFn& fetch) const;

  private:
    std::vector<Entry> entries;
    uint64_t           virt_size = 0, data_size = 0;
    DecompressFn       decompress;
};

}   // namespace install::cstore
