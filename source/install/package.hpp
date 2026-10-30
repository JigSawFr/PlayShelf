// package — a title's package, whatever brings it: an NSP or NSZ (a PFS0
// of NCAs, NCZs, tickets and certificates), or an XCI or XCZ (a game card
// image, whose root HFS0's "secure" partition holds the same), read
// through a Source of ranges (HTTP, a file, memory). What it lists is what
// the installer takes: the contents by id, the content metas, the tickets
// and their certificates.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "install/bytes.hpp"
#include "install/cnmt.hpp"

namespace install
{

// Where a package's bytes come from: any range, read into memory (the
// headers, a content meta), or streamed piece by piece (a content).
class Source
{
  public:
    virtual ~Source() = default;
    // The package's size, 0 when the source does not know it.
    virtual uint64_t size() const = 0;
    virtual Done     read(uint64_t offset, uint8_t* out, size_t n) = 0;
    // [offset, offset + length) to `sink`, in order, in pieces of any size;
    // why not when the source fails or `sink` returns false ("stopped").
    virtual Done stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink) = 0;
};

// The source of bytes in memory (the tests, a small download).
class MemorySource : public Source
{
  public:
    explicit MemorySource(std::vector<uint8_t> bytes, size_t piece_size = 0x10000) : data(std::move(bytes)), piece(piece_size) {}
    uint64_t size() const override { return data.size(); }
    Done     read(uint64_t offset, uint8_t* out, size_t n) override;
    Done     stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink) override;

  private:
    std::vector<uint8_t> data;
    size_t               piece;
};

enum class PackageKind { Nsp, Xci };

struct PackageFile
{
    std::string name;
    uint64_t    offset = 0;   // from the package's start
    uint64_t    size = 0;
};

struct Package
{
    PackageKind              kind = PackageKind::Nsp;
    std::vector<PackageFile> files;

    const PackageFile* find(const std::string& name) const;
    // A content's file: <id>.nca, <id>.ncz, <id>.cnmt.nca or <id>.cnmt.ncz.
    const PackageFile* content(const ContentId& id) const;
    std::vector<const PackageFile*> metas() const;     // *.cnmt.nca, *.cnmt.ncz
    std::vector<const PackageFile*> tickets() const;   // *.tik
};

// Reads the package's headers from `src`: a PFS0 at 0 is an NSP, "HEAD" at
// 0x100 a game card image (its root HFS0, then its "secure" partition).
[[nodiscard]] Parsed<Package> package_open(Source& src);

}   // namespace install
