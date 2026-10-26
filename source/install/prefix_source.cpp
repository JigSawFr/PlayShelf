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

bool PrefixSource::from_file(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                             std::string* why)
{
    if (fseeko(this->file, (off_t)offset, SEEK_SET) != 0) {
        if (why) *why = "the file fetched ahead could not be read";
        return false;
    }
    std::vector<uint8_t> buf((size_t)std::min<uint64_t>(PIECE, length));
    while (length) {
        const size_t n = (size_t)std::min<uint64_t>(buf.size(), length);
        if (std::fread(buf.data(), 1, n, this->file) != n) {
            if (why) *why = "the file fetched ahead could not be read";
            return false;
        }
        if (!sink(buf.data(), n)) {
            if (why && why->empty()) *why = "stopped";
            return false;
        }
        length -= n;
    }
    return true;
}

bool PrefixSource::read(uint64_t offset, uint8_t* out, size_t n, std::string* why)
{
    size_t got = 0;
    return this->stream(offset, n,
                        [&](const uint8_t* data, size_t size) {
                            std::memcpy(out + got, data, size);
                            got += size;
                            return true;
                        },
                        why) &&
           got == n;
}

bool PrefixSource::stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                          std::string* why)
{
    if (!length) return true;
    // The part in the file, then the part past it from the source.
    if (offset < this->have) {
        const uint64_t here = std::min(length, this->have - offset);
        if (!this->from_file(offset, here, sink, why)) return false;
        offset += here;
        length -= here;
        if (!length) return true;
    }
    return this->rest.stream(offset, length, sink, why);
}

}   // namespace install
