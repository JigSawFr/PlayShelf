// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/prefix_source.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace install
{
namespace
{
constexpr size_t PIECE = 256u << 10;   // read from the file at a time
}   // namespace

PrefixSource::PrefixSource(std::string path_, uint64_t have_, Source& rest_)
    : path(std::move(path_)), have(have_), rest(rest_)
{
    this->file = std::fopen(this->path.c_str(), "rb");
    if (!this->file) this->have = 0;   // nothing ahead after all: all from the source
}

PrefixSource::~PrefixSource()
{
    if (this->file) std::fclose(this->file);
}

Done PrefixSource::from_file(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink)
{
    if (fseeko(this->file, (off_t)offset, SEEK_SET) != 0) return fail("the file fetched ahead could not be read");
    std::vector<uint8_t> buf((size_t)std::min<uint64_t>(PIECE, length));
    while (length) {
        const size_t n = (size_t)std::min<uint64_t>(buf.size(), length);
        if (std::fread(buf.data(), 1, n, this->file) != n) return fail("the file fetched ahead could not be read");
        if (!sink(buf.data(), n)) return fail("stopped");
        length -= n;
    }
    return {};
}

Done PrefixSource::read(uint64_t offset, uint8_t* out, size_t n)
{
    size_t     got = 0;
    const Done r = this->stream(offset, n, [&](const uint8_t* data, size_t size) {
        std::memcpy(out + got, data, size);
        got += size;
        return true;
    });
    if (!r) return r;
    if (got != n) return fail("the range came out short");
    return {};
}

Done PrefixSource::stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink)
{
    if (!length) return {};
    // The part in the file, then the part past it from the source.
    if (offset < this->have) {
        const uint64_t here = std::min(length, this->have - offset);
        if (const Done r = this->from_file(offset, here, sink); !r) return r;
        offset += here;
        length -= here;
        if (!length) return {};
    }
    return this->rest.stream(offset, length, sink);
}

}   // namespace install
