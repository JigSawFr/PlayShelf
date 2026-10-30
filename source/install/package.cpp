// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/package.hpp"

#include <algorithm>

#include "install/bytes.hpp"
#include "install/pfs.hpp"

namespace install
{
namespace
{
std::optional<Package> fail(std::string* why, const std::string& text)
{
    if (why) *why = text;
    return std::nullopt;
}

// The archive at `offset` of `src`: its base header, then all of it.
std::optional<Pfs> read_pfs(Source& src, uint64_t offset, uint64_t limit, std::string* why)
{
    uint8_t base[PFS_BASE_HEADER];
    if (!src.read(offset, base, sizeof base, why)) return std::nullopt;
    const auto hs = pfs_header_size(base);
    if (!hs) {
        if (why) *why = "not a PFS0 or HFS0 archive, or a header too big";
        return std::nullopt;
    }
    std::vector<uint8_t> header((size_t)hs->size);
    if (!src.read(offset, header.data(), header.size(), why)) return std::nullopt;
    return pfs_parse(header, limit, why);
}
}   // namespace

bool MemorySource::read(uint64_t offset, uint8_t* out, size_t n, std::string* why)
{
    if (offset > data.size() || n > data.size() - offset) {
        if (why) *why = "read past the package's end";
        return false;
    }
    std::copy(data.begin() + (long)offset, data.begin() + (long)(offset + n), out);
    return true;
}

bool MemorySource::stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                          std::string* why)
{
    if (offset > data.size() || length > data.size() - offset) {
        if (why) *why = "read past the package's end";
        return false;
    }
    for (uint64_t at = 0; at < length;) {
        const size_t n = (size_t)std::min<uint64_t>(piece ? piece : length, length - at);
        if (!sink(data.data() + offset + at, n)) {
            if (why && why->empty()) *why = "stopped";
            return false;
        }
        at += n;
    }
    return true;
}

const PackageFile* Package::find(const std::string& name) const
{
    for (const PackageFile& f : files)
        if (f.name == name) return &f;
    return nullptr;
}

const PackageFile* Package::content(const ContentId& id) const
{
    const std::string hex = content_id_hex(id);
    for (const char* ext : {".nca", ".ncz", ".cnmt.nca", ".cnmt.ncz"})
        if (const PackageFile* f = find(hex + ext)) return f;
    return nullptr;
}

std::vector<const PackageFile*> Package::metas() const
{
    std::vector<const PackageFile*> out;
    for (const PackageFile& f : files)
        if (f.name.ends_with(".cnmt.nca") || f.name.ends_with(".cnmt.ncz")) out.push_back(&f);
    return out;
}

std::vector<const PackageFile*> Package::tickets() const
{
    std::vector<const PackageFile*> out;
    for (const PackageFile& f : files)
        if (f.name.ends_with(".tik")) out.push_back(&f);
    return out;
}

std::optional<Package> package_open(Source& src, std::string* why)
{
    const uint64_t size = src.size();
    uint8_t        head[XCI_HEADER];
    std::string    err;
    const bool     long_enough = !size || size >= XCI_HEADER;
    Package        pkg;
    if (long_enough && src.read(0, head, sizeof head, &err) && le32(head + 0x100) == 0x44414548) {
        // A game card image: the root HFS0, then its secure partition.
        const auto root = xci_root(head, size, why);
        if (!root) return std::nullopt;
        const auto rp = read_pfs(src, root->offset, size ? size - root->offset : 0, why);
        if (!rp) return std::nullopt;
        if (rp->kind != PfsKind::Hfs0) return fail(why, "a game card image whose root is not an HFS0");
        const PfsEntry* secure = nullptr;
        for (const PfsEntry& e : rp->entries)
            if (e.name == "secure") secure = &e;
        if (!secure) return fail(why, "a game card image without a secure partition");
        const uint64_t at = root->offset + secure->offset;
        const auto     sp = read_pfs(src, at, secure->size, why);
        if (!sp) return std::nullopt;
        pkg.kind = PackageKind::Xci;
        for (const PfsEntry& e : sp->entries)
            pkg.files.push_back(PackageFile{e.name, at + e.offset, e.size});
        return pkg;
    }
    const auto p = read_pfs(src, 0, size, why);
    if (!p) return std::nullopt;
    if (p->kind != PfsKind::Pfs0) return fail(why, "an HFS0 where an NSP's PFS0 was expected");
    pkg.kind = PackageKind::Nsp;
    for (const PfsEntry& e : p->entries)
        pkg.files.push_back(PackageFile{e.name, e.offset, e.size});
    return pkg;
}

}   // namespace install
