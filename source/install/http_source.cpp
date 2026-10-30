// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/http_source.hpp"

#include <cstring>

#include "install/split_stream.hpp"
#include "util/http.hpp"

namespace install
{

HttpSource::HttpSource(std::string url_, std::vector<std::string> headers_, const std::atomic<bool>* cancel_, std::string pin_,
                       int connections_)
    : url(std::move(url_)), headers(std::move(headers_)), cancel(cancel_), pin(std::move(pin_)), connections(connections_)
{
}

Done HttpSource::open()
{
    const auto got = http::resource_size(url, headers, pin);
    if (!got) return fail(got.error().error);
    total = *got;
    return {};
}

Done HttpSource::read(uint64_t offset, uint8_t* out, size_t n)
{
    if (total && (offset > total || n > total - offset)) return fail("read past the package's end");
    size_t     got = 0;
    const auto r = http::range(
        url, offset, n,
        [&](const uint8_t* data, size_t size) {
            std::memcpy(out + got, data, size);
            got += size;
            return true;
        },
        headers, cancel, pin);
    if (!r) return fail(r.error().error);
    if (got != n) return fail("the range came out short");
    return {};
}

Done HttpSource::stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink)
{
    if (total && (offset > total || length > total - offset)) return fail("read past the package's end");
    const auto fetch = [this](uint64_t at, uint64_t n, const std::function<bool(const uint8_t*, size_t)>& to,
                              const std::atomic<bool>* stop) -> Done {
        const auto r = http::range(url, at, n, to, headers, stop, pin);
        if (!r) return fail(r.error().error);
        return {};
    };
    return split_stream(offset, length, connections, fetch, sink, cancel);
}

}   // namespace install
