// ivfc — the integrity tree of a RomFS section's file system header: five
// levels of SHA-256 hashes over the data, each hash level over the one below
// it in blocks of 0x4000 bytes, and the master hash of the top one. The
// header (fs header 0x08..0xE8) keeps where each level is in the section.
//
// What Builder lays out is the order the console reads whatever the header
// says: the data first, then the hash levels from the one nearest the data
// up to the top; each hash level is padded to the block size, with a whole
// extra block when it was already a multiple of it (as the tools that pack
// homebrew do). The data level's size is its real size. A section whose data
// is not known before it is made (compressed blocks and their table) has to
// be laid out this way, since the hash levels depend on all of it.
//
// Plain C++ with the kit's SHA-256.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "install/bytes.hpp"
#include "util/sha256.hpp"

namespace install::ivfc
{

constexpr uint32_t BLOCK = 0x4000;
constexpr uint32_t BLOCK_ORDER = 14;
constexpr size_t   HASH = 0x20;
constexpr size_t   LEVELS = 6;   // five of hashes, then the data

struct Layout
{
    std::array<uint64_t, LEVELS> offset{}, size{};   // in the section; the last level is the data
    std::array<uint32_t, LEVELS> order{};            // block size = 1 << order
    std::array<uint8_t, HASH>    master{};           // SHA-256 of the top level
};

struct Result
{
    std::vector<uint8_t> hash_levels;   // the five levels in file order, to put right after the data
    Layout               layout;
};

// Hashes data fed in pieces of any size; the last block, short, is hashed as
// if padded with zeros. finish() puts the hash levels after the data, which
// starts the section.
class Builder
{
  public:
    void   add(std::span<const uint8_t> data);
    // The data level's real size is what was fed. Spent afterwards.
    Result finish();

  private:
    void block(std::span<const uint8_t> b);

    std::vector<uint8_t> partial, hashes;
    uint64_t             total = 0;
};

// The size of the hash levels a data level of `data_size` bytes takes.
uint64_t hash_levels_size(uint64_t data_size);

// The IVFC fields of a section's 0x200-byte file system header.
void           write_fs_header(uint8_t fs_header[0x200], const Layout& l);
Parsed<Layout> read_fs_header(const uint8_t fs_header[0x200]);

// Reads `out.size()` bytes of the section at `offset` (decrypted).
using ReadFn = std::function<bool(uint64_t offset, std::span<uint8_t> out)>;
// Checks every hash of the tree against the section's bytes, the master hash
// first; why not when one differs or a read fails.
[[nodiscard]] Done verify(const Layout& l, const ReadFn& read);

}   // namespace install::ivfc
