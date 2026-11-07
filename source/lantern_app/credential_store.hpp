// What this console keeps for each paired Lantern server (the SDK's
// CredentialStore): its token, the device key the token is bound to and the
// server key it pinned, by the server's `uid`, in lantern.json on the SD
// card. The token and the key's seed are sealed for this console
// (secret_box); the pinned key is public. schema 1: a newer file is read as
// absent and never written over; a key this build does not know is kept.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include <optional>
#include <string_view>

#include <lantern/credentials.hpp>
#include <lantern/sha256.hpp>

namespace lantern_app
{

constexpr int CREDENTIALS_SCHEMA = 1;

// lantern.json in paths::data_dir().
std::string credentials_file();

class FileCredentialStore final : public lantern::CredentialStore
{
  public:
    FileCredentialStore() : FileCredentialStore(credentials_file()) {}
    explicit FileCredentialStore(std::string path) : path_(std::move(path)) {}

    lantern::Result<std::optional<lantern::Credentials>> load(std::string_view uid, lantern::Allocator allocator) override;
    lantern::Result<void>                                store(std::string_view uid, const lantern::Credentials& credentials) override;
    lantern::Result<void>                                erase(std::string_view uid) override;

  private:
    std::string path_;
};

}   // namespace lantern_app
