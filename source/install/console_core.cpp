// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/console_core.hpp"

#include <cstring>
#include <format>
#include <utility>
#include <vector>

namespace install
{
namespace
{
CoreMetaKey core_key(const MetaKey& k)
{
    CoreMetaKey c{};
    c.id = k.id;
    c.version = k.version;
    c.type = std::to_underlying(k.type);
    return c;
}

bool check(Result rc, const char* what, std::string* why)
{
    if (R_SUCCEEDED(rc)) return true;
    if (why) *why = result_text(rc, what);
    return false;
}
}   // namespace

std::string result_text(Result rc, const char* what)
{
    return std::format("{}: {:04}-{:04}", what, 2000 + (unsigned)(rc & 0x1ff), (unsigned)((rc >> 9) & 0x1fff));
}

CoreConsole::CoreConsole(CoreStorage storage)
{
    opened = check(core_install_open(storage), "the console's install services", &open_error);
}

CoreConsole::~CoreConsole()
{
    if (opened) core_install_close();
}

bool CoreConsole::has_content(const ContentId& id)
{
    bool has = false;
    return R_SUCCEEDED(core_content_has(id.data(), &has)) && has;
}

bool CoreConsole::create_placeholder(const ContentId& id, uint64_t size, std::string* why)
{
    return check(core_placeholder_create(id.data(), size), "creating a placeholder", why);
}

bool CoreConsole::write_placeholder(const ContentId& id, uint64_t offset, const uint8_t* data, size_t size, std::string* why)
{
    return check(core_placeholder_write(id.data(), offset, data, size), "writing a placeholder", why);
}

bool CoreConsole::register_placeholder(const ContentId& id, std::string* why)
{
    return check(core_placeholder_register(id.data()), "registering a content", why);
}

bool CoreConsole::has_placeholder(const ContentId& id)
{
    bool has = false;
    return R_SUCCEEDED(core_placeholder_has(id.data(), &has)) && has;
}

std::vector<ContentId> CoreConsole::placeholders()
{
    // An install leaves one at most; a few others' installers may have too.
    constexpr int              MAX = 256;
    std::vector<uint8_t>       raw(16 * MAX);
    int                        count = 0;
    std::vector<ContentId>     out;
    if (R_FAILED(core_placeholder_list(reinterpret_cast<u8(*)[16]>(raw.data()), MAX, &count))) return out;
    for (int i = 0; i < count; i++) {
        ContentId id;
        std::memcpy(id.data(), raw.data() + 16 * i, 16);
        out.push_back(id);
    }
    return out;
}

void CoreConsole::delete_placeholder(const ContentId& id) { core_placeholder_delete(id.data()); }
void CoreConsole::delete_content(const ContentId& id) { core_content_delete(id.data()); }

uint64_t CoreConsole::free_space()
{
    u64 free_bytes = 0;
    return R_SUCCEEDED(core_free_space(&free_bytes)) ? free_bytes : 0;
}

std::optional<std::vector<uint8_t>> CoreConsole::get_meta(const MetaKey& key)
{
    const CoreMetaKey k = core_key(key);
    u64               size = 0;
    bool              found = false;
    if (R_FAILED(core_meta_get(&k, nullptr, 0, &size, &found)) || !found) return std::nullopt;
    std::vector<uint8_t> rec((size_t)size);
    if (R_FAILED(core_meta_get(&k, rec.data(), rec.size(), &size, &found)) || !found) return std::nullopt;
    return rec;
}

bool CoreConsole::set_meta(const MetaKey& key, const std::vector<uint8_t>& record, std::string* why)
{
    const CoreMetaKey k = core_key(key);
    return check(core_meta_set(&k, record.data(), record.size()), "setting a content meta record", why);
}

bool CoreConsole::remove_meta(const MetaKey& key, std::string* why)
{
    const CoreMetaKey k = core_key(key);
    return check(core_meta_remove(&k), "removing a content meta record", why);
}

bool CoreConsole::commit_meta(std::string* why)
{
    return check(core_meta_commit(), "committing the content meta records", why);
}

bool CoreConsole::push_application_record(uint64_t application_id, const MetaKey& key, std::string* why)
{
    const CoreMetaKey k = core_key(key);
    return check(core_push_application_record(application_id, &k), "pushing the application record", why);
}

bool CoreConsole::import_ticket(const std::vector<uint8_t>& ticket, const std::vector<uint8_t>& certificate,
                                std::string* why)
{
    return check(core_import_ticket(ticket.data(), ticket.size(), certificate.data(), certificate.size()),
                 "importing a ticket", why);
}

uint32_t CoreConsole::system_version() { return core_system_version(); }

Parsed<NcaKeys> core_keys()
{
    NcaKeys     k;
    std::string why;
    if (!check(core_header_key(k.header_key.data()), "deriving the header key", &why)) return fail(why);
    k.unwrap = [](uint8_t generation, uint8_t index, const uint8_t in[16], uint8_t out[16]) {
        return R_SUCCEEDED(core_unwrap_key(generation, index, in, out));
    };
    return k;
}

std::map<uint64_t, uint32_t> installed_versions(std::string* why)
{
    // A console holds a few thousand at most: games, their updates, their
    // add-ons.
    constexpr int            MAX = 8192;
    std::vector<CoreMetaKey> keys(MAX);
    int                      count = 0;
    std::map<uint64_t, uint32_t> out;
    const Result             rc = core_installed_list(keys.data(), MAX, &count);
    if (R_FAILED(rc)) {
        if (why) *why = result_text(rc, "ncm");
        return out;
    }
    for (int i = 0; i < count; i++) {
        auto it = out.find(keys[(size_t)i].id);
        if (it == out.end()) out[keys[(size_t)i].id] = keys[(size_t)i].version;
        else if (it->second < keys[(size_t)i].version) it->second = keys[(size_t)i].version;
    }
    return out;
}

}   // namespace install
