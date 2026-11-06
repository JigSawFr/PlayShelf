// A package read where the installer asks, from a Lantern server: each read
// is a signed Range request (Client::read_range), so the install goes on as the
// bytes arrive and nothing is written to the SD card first. What it reads is
// not hashed whole (the installer reads out of order); the package's contents
// are checked by the installer, and the transport by TLS and the signatures.
//
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>
#include <string>

#include "install/package.hpp"
#include "lantern_app/package.hpp"

namespace lantern_app
{

class RangeSource : public install::Source
{
  public:
    // `client`: open_client()'s, which must not outlive its platform.
    RangeSource(std::unique_ptr<lantern::Client> client, PackageRequest request);

    uint64_t                    size() const override { return request.size; }
    install::Done               read(uint64_t offset, uint8_t* out, size_t n) override;
    install::Done               stream(uint64_t offset, uint64_t length,
                                       const std::function<bool(const uint8_t*, size_t)>& sink) override;

  private:
    std::unique_ptr<lantern::Client> client;
    PackageRequest                   request;
};

}   // namespace lantern_app
