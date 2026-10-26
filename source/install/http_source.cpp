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

bool HttpSource::open(std::string* why)
{
    return http::resource_size(url, &total, why, headers, pin);
}

bool HttpSource::read(uint64_t offset, uint8_t* out, size_t n, std::string* why)
{
    if (total && (offset > total || n > total - offset)) {
        if (why) *why = "read past the package's end";
        return false;
    }
    size_t got = 0;
    const bool ok = http::range(
        url, offset, n,
        [&](const uint8_t* data, size_t size) {
            std::memcpy(out + got, data, size);
            got += size;
            return true;
        },
        why, headers, cancel, pin);
    return ok && got == n;
}

bool HttpSource::stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                        std::string* why)
{
    if (total && (offset > total || length > total - offset)) {
        if (why) *why = "read past the package's end";
        return false;
    }
    const auto fetch = [this](uint64_t at, uint64_t n, const std::function<bool(const uint8_t*, size_t)>& to, std::string* error,
                              const std::atomic<bool>* stop) { return http::range(url, at, n, to, error, headers, stop, pin); };
    return split_stream(offset, length, connections, fetch, sink, why, cancel);
}

}   // namespace install
