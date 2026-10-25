// console_core — install::Console over core/install.h (libnx on the Switch,
// the folder console of sim/ on a PC), and the keys the console derives.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "core/install.h"
#include "install/installer.hpp"

namespace install
{

class CoreConsole : public Console
{
  public:
    // Opens the services and `storage`; ok() says whether that worked.
    explicit CoreConsole(CoreStorage storage);
    ~CoreConsole() override;
    bool               ok() const { return opened; }
    const std::string& error() const { return open_error; }

    bool     has_content(const ContentId& id) override;
    bool     create_placeholder(const ContentId& id, uint64_t size, std::string* why) override;
    bool     write_placeholder(const ContentId& id, uint64_t offset, const uint8_t* data, size_t size,
                               std::string* why) override;
    bool     register_placeholder(const ContentId& id, std::string* why) override;
    bool     has_placeholder(const ContentId& id) override;
    std::vector<ContentId> placeholders() override;
    void     delete_placeholder(const ContentId& id) override;
    void     delete_content(const ContentId& id) override;
    uint64_t free_space() override;
    std::optional<std::vector<uint8_t>> get_meta(const MetaKey& key) override;
    bool set_meta(const MetaKey& key, const std::vector<uint8_t>& record, std::string* why) override;
    bool remove_meta(const MetaKey& key, std::string* why) override;
    bool commit_meta(std::string* why) override;
    bool push_application_record(uint64_t application_id, const MetaKey& key, std::string* why) override;
    bool import_ticket(const std::vector<uint8_t>& ticket, const std::vector<uint8_t>& certificate,
                       std::string* why) override;
    uint32_t system_version() override;

  private:
    bool        opened = false;
    std::string open_error;
};

// The console's NCA keys: its header key, and its key area keys through
// core_unwrap_key. Nothing when the header key cannot be had.
std::optional<NcaKeys> core_keys(std::string* why = nullptr);

// "2002-0003 (…)": a Result as the console's error codes read, and what it
// failed at.
std::string result_text(Result rc, const char* what);

// What the console has installed (core_installed_list): each title id, base
// game, update or add-on, with its highest version. Empty, `why` set, when
// it cannot be read.
std::map<uint64_t, uint32_t> installed_versions(std::string* why = nullptr);

}   // namespace install
