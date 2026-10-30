// pfs — the Switch's two flat archive formats, one parser: PFS0 (an NSP, and
// the file system inside an NCA's sections) and HFS0 (a game card image's
// partitions, XCI), which differ only in their magic and in the size of an
// entry (HFS0 adds a hash of the file's first bytes). And the game card
// image's header, which says where its root HFS0 is.
//
// Every length and offset comes from the file, so each is checked against
// what holds it: entries and names within the header, names ended within the
// string table, a file's data within the archive when its size is known. A
// header bigger than MAX_HEADER is refused before anything is read for it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace install
{

enum class PfsKind { Pfs0, Hfs0 };

struct PfsEntry
{
    std::string             name;
    uint64_t                offset = 0;   // from the archive's start (its header included)
    uint64_t                size = 0;
    uint32_t                hashed_size = 0;   // HFS0: the first bytes `hash` covers
    std::array<uint8_t, 32> hash{};            // HFS0: their SHA-256
};

struct Pfs
{
    PfsKind               kind = PfsKind::Pfs0;
    uint64_t              header_size = 0;   // where the files' data starts
    std::vector<PfsEntry> entries;

    const PfsEntry* find(const std::string& name) const;
    // The entries whose name ends with `suffix` (".cnmt.nca", ".tik").
    std::vector<const PfsEntry*> ending_with(const std::string& suffix) const;
};

constexpr uint64_t PFS_BASE_HEADER = 0x10;
constexpr uint64_t MAX_HEADER = 16u << 20;   // far above any real one (an NSP's: a few hundred bytes)
constexpr uint32_t MAX_ENTRIES = 0x10000;

// From the 16-byte base header: the kind and the whole header's size, so a
// reader fetches just that much. Nothing when the magic is neither, or the
// size is past MAX_HEADER.
struct PfsHeaderSize
{
    PfsKind  kind;
    uint64_t size;
};
std::optional<PfsHeaderSize> pfs_header_size(const uint8_t* base, size_t size);

// The whole header (at least pfs_header_size's size). archive_size, when
// known (non-zero), bounds the files' data. Nothing, with `why` set, when
// it is not a well-formed archive.
[[nodiscard]] std::optional<Pfs> pfs_parse(const uint8_t* data, size_t size, uint64_t archive_size = 0, std::string* why = nullptr);

// A game card image (XCI): where its root HFS0 is, from the first 0x200
// bytes ("HEAD" at 0x100). Only the image's own claims are checked here
// (within image_size when known); the HFS0 is parsed as above.
struct XciRoot
{
    uint64_t offset = 0;
    uint64_t header_size = 0;   // the root HFS0's header, as the card header gives it
};
constexpr size_t XCI_HEADER = 0x200;
[[nodiscard]] std::optional<XciRoot> xci_root(const uint8_t* header, size_t size, uint64_t image_size = 0, std::string* why = nullptr);

}   // namespace install
