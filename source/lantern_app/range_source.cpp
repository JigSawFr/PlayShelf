// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/range_source.hpp"

#include <algorithm>
#include <vector>

namespace lantern_app
{
namespace
{
constexpr uint64_t PIECE = 512 * 1024;   // one request's bytes, at most
}

RangeSource::RangeSource(std::unique_ptr<lantern::Client> c, PackageRequest r) : client(std::move(c)), request(std::move(r)) {}

install::Done RangeSource::read(uint64_t offset, uint8_t* out, size_t n)
{
    if (offset > request.size || n > request.size - offset) return std::unexpected("a read past the end of the package");
    if (n == 0) return {};
    const lantern::DownloadTarget target{request.url, request.size, {}};
    uint64_t                      done = 0;
    while (done < n) {
        const uint64_t len = std::min<uint64_t>(n - done, PIECE);
        const auto     got = client->read_range(target, lantern::ByteRange{offset + done, offset + done + len - 1},
                                                std::span<std::byte>(reinterpret_cast<std::byte*>(out) + done, (size_t)len));
        if (!got) return std::unexpected(describe(got.error()));
        done += len;
    }
    return {};
}

install::Done RangeSource::stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink)
{
    std::vector<uint8_t> piece((size_t)std::min<uint64_t>(length, PIECE));
    uint64_t             done = 0;
    while (done < length) {
        const size_t n = (size_t)std::min<uint64_t>(length - done, piece.size());
        if (const auto got = read(offset + done, piece.data(), n); !got) return got;
        done += n;
        if (!sink(piece.data(), n)) return std::unexpected("stopped");
    }
    return {};
}

}   // namespace lantern_app
