// console_core — install::Console over core/install.h (libnx on the Switch,
// the folder console of sim/ on a PC), and the keys the console derives.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

#include "core/install.h"
#include "install/bytes.hpp"
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
    [[nodiscard]] Done create_placeholder(const ContentId& id, uint64_t size) override;
    [[nodiscard]] Done write_placeholder(const ContentId& id, uint64_t offset, const uint8_t* data, size_t size) override;
    [[nodiscard]] Done register_placeholder(const ContentId& id) override;
    bool     has_placeholder(const ContentId& id) override;
    [[nodiscard]] Done register_placeholder_as(const ContentId& placeholder, const ContentId& content) override;
    std::optional<uint64_t> content_size(const ContentId& id) override;
    [[nodiscard]] Done read_content(const ContentId& id, uint64_t offset, std::span<uint8_t> out) override;
    std::vector<ContentId> placeholders() override;
    void     delete_placeholder(const ContentId& id) override;
    void     delete_content(const ContentId& id) override;
    uint64_t free_space() override;
    std::optional<std::vector<uint8_t>> get_meta(const MetaKey& key) override;
    [[nodiscard]] Done set_meta(const MetaKey& key, const std::vector<uint8_t>& record) override;
    [[nodiscard]] Done remove_meta(const MetaKey& key) override;
    [[nodiscard]] Done commit_meta() override;
    [[nodiscard]] Done push_application_record(uint64_t application_id, const MetaKey& key) override;
    [[nodiscard]] Done replace_application_record(uint64_t application_id, const std::vector<MetaKey>& keys) override;
    void               application_changed(uint64_t application_id, uint32_t launch_version) override;
    [[nodiscard]] Parsed<bool> read_through_system(uint64_t application_id, const std::function<bool()>& stop,
                                                   const std::function<void(uint64_t, uint64_t)>& progress) override;
    [[nodiscard]] Done import_ticket(const std::vector<uint8_t>& ticket, const std::vector<uint8_t>& certificate) override;
    uint32_t system_version() override;

  private:
    bool        opened = false;
    std::string open_error;
};

// The console's NCA keys: its header key, and its key area keys through
// core_unwrap_key. Nothing when the header key cannot be had.
[[nodiscard]] Parsed<NcaKeys> core_keys();

// "2002-0003 (…)": a Result as the console's error codes read, and what it
// failed at.
std::string result_text(Result rc, const char* what);

// What the console has installed (core_installed_list): each title id, base
// game, update or add-on, with its highest version; why not when it cannot
// be read.
[[nodiscard]] Parsed<std::map<uint64_t, uint32_t>> installed_versions();

}   // namespace install
