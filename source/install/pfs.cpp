// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/pfs.hpp"

#include <cstring>

#include "install/bytes.hpp"

namespace install
{
namespace
{
constexpr uint32_t MAGIC_PFS0 = 0x30534650;   // "PFS0"
constexpr uint32_t MAGIC_HFS0 = 0x30534648;   // "HFS0"
constexpr uint32_t MAGIC_HEAD = 0x44414548;   // "HEAD"

uint64_t entry_size(PfsKind k) { return k == PfsKind::Pfs0 ? 0x18 : 0x40; }

template <class T> std::optional<T> fail(std::string* why, const char* text)
{
    if (why) *why = text;
    return std::nullopt;
}
}   // namespace

const PfsEntry* Pfs::find(const std::string& name) const
{
    for (const PfsEntry& e : entries)
        if (e.name == name) return &e;
    return nullptr;
}

std::vector<const PfsEntry*> Pfs::ending_with(const std::string& suffix) const
{
    std::vector<const PfsEntry*> out;
    for (const PfsEntry& e : entries)
        if (e.name.size() >= suffix.size() && e.name.compare(e.name.size() - suffix.size(), suffix.size(), suffix) == 0)
            out.push_back(&e);
    return out;
}

std::optional<PfsHeaderSize> pfs_header_size(const uint8_t* base, size_t size)
{
    if (size < PFS_BASE_HEADER) return std::nullopt;
    const uint32_t magic = le32(base);
    PfsKind        kind;
    if (magic == MAGIC_PFS0)
        kind = PfsKind::Pfs0;
    else if (magic == MAGIC_HFS0)
        kind = PfsKind::Hfs0;
    else
        return std::nullopt;
    const uint32_t files = le32(base + 4), strings = le32(base + 8);
    if (files > MAX_ENTRIES) return std::nullopt;
    const uint64_t total = PFS_BASE_HEADER + (uint64_t)files * entry_size(kind) + strings;
    if (total > MAX_HEADER) return std::nullopt;
    return PfsHeaderSize{kind, total};
}

std::optional<Pfs> pfs_parse(const uint8_t* data, size_t size, uint64_t archive_size, std::string* why)
{
    const auto hs = pfs_header_size(data, size);
    if (!hs) return fail<Pfs>(why, "not a PFS0 or HFS0 header, or one too big");
    if (size < hs->size) return fail<Pfs>(why, "header cut short");
    if (archive_size && hs->size > archive_size) return fail<Pfs>(why, "header past the archive's end");

    Pfs p;
    p.kind = hs->kind;
    p.header_size = hs->size;
    const uint32_t files = le32(data + 4), strings = le32(data + 8);
    const uint64_t esize = entry_size(p.kind);
    const uint8_t* table = data + PFS_BASE_HEADER + (uint64_t)files * esize;
    p.entries.reserve(files);
    for (uint32_t i = 0; i < files; i++) {
        const uint8_t* e = data + PFS_BASE_HEADER + (uint64_t)i * esize;
        PfsEntry       entry;
        const uint64_t data_offset = le64(e), file_size = le64(e + 8);
        const uint32_t name_at = le32(e + 16);
        if (name_at >= strings) return fail<Pfs>(why, "a name past the string table");
        const void* end = std::memchr(table + name_at, 0, strings - name_at);
        if (!end) return fail<Pfs>(why, "a name not ended within the string table");
        entry.name.assign((const char*)table + name_at, (const char*)end);
        if (entry.name.empty()) return fail<Pfs>(why, "an empty name");
        uint64_t start, stop;
        if (!add_ok(p.header_size, data_offset, &start) || !add_ok(start, file_size, &stop))
            return fail<Pfs>(why, "a file's offset or size out of range");
        if (archive_size && stop > archive_size) return fail<Pfs>(why, "a file past the archive's end");
        entry.offset = start;
        entry.size = file_size;
        if (p.kind == PfsKind::Hfs0) {
            entry.hashed_size = le32(e + 20);
            if (entry.hashed_size > file_size) return fail<Pfs>(why, "a hashed size past the file's");
            std::memcpy(entry.hash.data(), e + 32, 32);
        }
        p.entries.push_back(std::move(entry));
    }
    return p;
}

std::optional<XciRoot> xci_root(const uint8_t* header, size_t size, uint64_t image_size, std::string* why)
{
    if (size < XCI_HEADER) return fail<XciRoot>(why, "game card header cut short");
    if (le32(header + 0x100) != MAGIC_HEAD) return fail<XciRoot>(why, "not a game card image (no HEAD)");
    XciRoot r;
    r.offset = le64(header + 0x130);
    r.header_size = le64(header + 0x138);
    if (r.offset < XCI_HEADER) return fail<XciRoot>(why, "root partition inside the card header");
    if (r.header_size < PFS_BASE_HEADER || r.header_size > MAX_HEADER) return fail<XciRoot>(why, "root partition header size out of range");
    uint64_t end;
    if (!add_ok(r.offset, r.header_size, &end) || (image_size && end > image_size))
        return fail<XciRoot>(why, "root partition past the image's end");
    return r;
}

}   // namespace install
