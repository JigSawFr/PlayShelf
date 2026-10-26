// prefix_source — a package whose first bytes are in a file already (the
// downloads fetched ahead, action/download_flow) and the rest at its
// source: what lies in the file read from it, what lies past it asked of
// the source, a range across both handed on in order.
//
// Plain C++ for the host tests (tests/prefix_source).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdio>
#include <string>

#include "install/package.hpp"

namespace install
{

class PrefixSource : public Source
{
  public:
    // `path` holds the package's first `have` bytes; `rest` is the package
    // whole (its size, the bytes past `have`).
    PrefixSource(std::string path, uint64_t have, Source& rest);
    ~PrefixSource() override;
    PrefixSource(const PrefixSource&) = delete;
    PrefixSource& operator=(const PrefixSource&) = delete;

    uint64_t size() const override { return rest.size(); }
    bool     read(uint64_t offset, uint8_t* out, size_t n, std::string* why) override;
    bool     stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                    std::string* why) override;

  private:
    // [offset, offset + length), all of it before `have`, from the file.
    bool from_file(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                   std::string* why);

    std::string path;
    uint64_t    have;
    Source&     rest;
    FILE*       file = nullptr;
};

}   // namespace install
