// bktr — the patch layers of an update's RomFS section: the part of an NCA
// that makes a game's update a patch over its base game's RomFS rather than a
// RomFS of its own. The section (encryption type "BKTR", fs header 0x100 patch
// info) holds, in this order:
//
//   the patch data, then the relocation table ("indirect"), then the
//   subsection table ("AesCtrEx"), the last one at the offset the patch info
//   gives; everything before the subsection table is AES-CTR encrypted with a
//   counter whose upper half carries a *generation* per subsection; the
//   subsection table itself with the section's plain counter.
//
// The RomFS the game sees is the relocation table's virtual storage: each
// entry says that, from its virtual offset, bytes come from the base game's
// section (storage 0, decrypted and raw, hash levels included, at the physical
// offset) or from this update's patch data (storage 1). The integrity tree of
// the update's fs header describes that virtual storage, which is what makes
// it checkable: a patched section read here verifies against the hashes
// Nintendo's tools wrote or it is not read right.
//
// Both tables are bucket trees of 16 KiB nodes like the compression layer's
// (compressed_storage.hpp), with entries of 0x14 bytes (relocation: virtual
// offset s64, physical offset s64, storage index s32) and 0x10 bytes
// (subsection: offset s64, encryption u8 [0 encrypted, 1 not], 3 reserved,
// generation s32). Only a tree of one 16 KiB node of offsets is read (up to
// 2046 entry sets); a bigger one is refused.
//
// Plain C++. The layout is written from the format's description (Atmosphere's
// reader was the reference for the rules enforced); a patch that reads here is
// not proof the firmware reads it the same.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "install/bytes.hpp"
#include "install/nca.hpp"

namespace install::bktr
{

constexpr uint32_t NODE_SIZE = 0x4000;
constexpr uint32_t NODE_HEADER = 0x10;
constexpr uint32_t RELOCATION_ENTRY = 0x14;
constexpr uint32_t SUBSECTION_ENTRY = 0x10;
constexpr size_t   PATCH_INFO_AT = 0x100;   // in the file system header

struct TreeHeader
{
    uint32_t magic = 0, version = 0;
    int32_t  entry_count = 0;
};

struct PatchInfo
{
    uint64_t   indirect_offset = 0, indirect_size = 0;
    TreeHeader indirect;
    uint64_t   aes_offset = 0, aes_size = 0;
    TreeHeader aes;

    bool has_indirect() const { return indirect_size != 0; }
    bool has_aes() const { return aes_size != 0; }
};
// The patch info of a file system header; it checks the extents against each
// other and against the section's size.
[[nodiscard]] Parsed<PatchInfo> read_patch_info(const uint8_t fs_header[0x200], uint64_t section_size);

// A bucket tree's entries, validated: flat, ordered by their first 8 bytes.
struct Tree
{
    size_t               entry_size = 0;
    std::vector<uint8_t> entries;
    uint64_t             end = 0;   // where the last entry ends

    size_t         count() const { return entries.size() / entry_size; }
    const uint8_t* at(size_t i) const { return entries.data() + i * entry_size; }
    int64_t        offset(size_t i) const { return (int64_t)le64(at(i)); }
};
uint64_t table_size(uint32_t entry_count, size_t entry_size);
[[nodiscard]] Parsed<Tree> parse_tree(std::span<const uint8_t> table, int32_t entry_count, size_t entry_size);

using ReadFn = std::function<bool(uint64_t offset, std::span<uint8_t> out)>;

// What a patched section is made from.
struct PatchSource
{
    ReadFn                  nca;               // the update NCA, as stored (encrypted)
    uint64_t                section_start = 0; // in the NCA
    uint64_t                section_size = 0;
    std::array<uint8_t, 16> key{};
    uint64_t                upper_iv = 0;      // fs header 0x140: generation low, secure value high
    bool                    encrypted = true;  // encryption type BKTR; false for "none"
    PatchInfo               info;
    ReadFn                  base;              // the base game's same section, decrypted, raw
    uint64_t                base_size = 0;
};

class PatchedSection
{
  public:
    // Reads both tables; why not when they are not what the rules above ask.
    [[nodiscard]] static Parsed<PatchedSection> open(PatchSource src);

    uint64_t size() const { return virt_size; }   // the virtual storage, hash levels included
    // The patched section's bytes at [offset, offset + out.size()).
    [[nodiscard]] bool read(uint64_t offset, std::span<uint8_t> out) const;

    const Tree& relocations() const { return indirect; }
    const Tree& subsections() const { return aes; }

  private:
    bool read_patch(uint64_t offset, std::span<uint8_t> out) const;   // [0, indirect offset) and the relocation table, decrypted

    PatchSource src;
    Tree        indirect, aes;
    uint64_t    virt_size = 0;
};

}   // namespace install::bktr
