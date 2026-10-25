// http_source — a package on an HTTPS server as an install::Source: its
// headers read with Range requests, its contents streamed as they arrive.
// The server must serve ranges (a 206 for each request), as AeroFoil,
// Ownfoil and any static file server do.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "install/package.hpp"

namespace install
{

class HttpSource : public Source
{
  public:
    // headers: sent with every request (an Authorization). cancel: as for
    // http::range().
    HttpSource(std::string url, std::vector<std::string> headers = {}, const std::atomic<bool>* cancel = nullptr);
    // Asks the server for the package's size; false (why set) when it
    // cannot be had or the server serves no ranges.
    bool     open(std::string* why);
    uint64_t size() const override { return total; }
    bool     read(uint64_t offset, uint8_t* out, size_t n, std::string* why) override;
    bool     stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
                    std::string* why) override;

  private:
    std::string              url;
    std::vector<std::string> headers;
    const std::atomic<bool>* cancel;
    uint64_t                 total = 0;
};

}   // namespace install
