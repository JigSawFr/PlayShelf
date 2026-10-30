// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/package.hpp"

#include <algorithm>

#include "install/bytes.hpp"
#include "install/pfs.hpp"

namespace install
{
namespace
{
// The archive at `offset` of `src`: its base header, then all of it.
Parsed<Pfs> read_pfs(Source& src, uint64_t offset, uint64_t limit)
{
    uint8_t base[PFS_BASE_HEADER];
    if (const Done r = src.read(offset, base, sizeof base); !r) return fail(r.error());
    const auto hs = pfs_header_size(base);
    if (!hs) return fail("not a PFS0 or HFS0 archive, or a header too big");
    std::vector<uint8_t> header((size_t)hs->size);
    if (const Done r = src.read(offset, header.data(), header.size()); !r) return fail(r.error());
    return pfs_parse(header, limit);
}
}   // namespace

Done MemorySource::read(uint64_t offset, uint8_t* out, size_t n)
{
    if (offset > data.size() || n > data.size() - offset) return fail("read past the package's end");
    std::copy(data.begin() + (long)offset, data.begin() + (long)(offset + n), out);
    return {};
}

Done MemorySource::stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink)
{
    if (offset > data.size() || length > data.size() - offset) return fail("read past the package's end");
    for (uint64_t at = 0; at < length;) {
        const size_t n = (size_t)std::min<uint64_t>(piece ? piece : length, length - at);
        if (!sink(data.data() + offset + at, n)) return fail("stopped");
        at += n;
    }
    return {};
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

Parsed<Package> package_open(Source& src)
{
    const uint64_t size = src.size();
    uint8_t        head[XCI_HEADER];
    const bool     long_enough = !size || size >= XCI_HEADER;
    Package        pkg;
    if (long_enough && src.read(0, head, sizeof head) && le32(head + 0x100) == 0x44414548) {
        // A game card image: the root HFS0, then its secure partition.
        const auto root = xci_root(head, size);
        if (!root) return fail(root.error());
        const auto rp = read_pfs(src, root->offset, size ? size - root->offset : 0);
        if (!rp) return fail(rp.error());
        if (rp->kind != PfsKind::Hfs0) return fail("a game card image whose root is not an HFS0");
        const PfsEntry* secure = nullptr;
        for (const PfsEntry& e : rp->entries)
            if (e.name == "secure") secure = &e;
        if (!secure) return fail("a game card image without a secure partition");
        const uint64_t at = root->offset + secure->offset;
        const auto     sp = read_pfs(src, at, secure->size);
        if (!sp) return fail(sp.error());
        pkg.kind = PackageKind::Xci;
        for (const PfsEntry& e : sp->entries)
            pkg.files.push_back(PackageFile{e.name, at + e.offset, e.size});
        return pkg;
    }
    const auto p = read_pfs(src, 0, size);
    if (!p) return fail(p.error());
    if (p->kind != PfsKind::Pfs0) return fail("an HFS0 where an NSP's PFS0 was expected");
    pkg.kind = PackageKind::Nsp;
    for (const PfsEntry& e : p->entries)
        pkg.files.push_back(PackageFile{e.name, e.offset, e.size});
    return pkg;
}

}   // namespace install
