// Simulated core/install.h: a console in a folder of the desktop build's data
// (playshelf_data/sim_console/), so an install runs end to end on a PC.
//   placeholders/<id>   a placeholder being written
//   contents/<id>.nca   a registered content
//   meta/<id>-<version>.bin  a content meta record (committed)
//   records.log         the application records pushed, one a line
//   tickets/<n>.tik, .cert  the tickets imported
// Its keys are the host tests' (tests/install/builders.hpp): it installs the
// packages those build, not a console's. PLAYSHELF_SIM_FREE_MB sets its
// free space (32 GiB by default); the system version is the simulated one
// (PLAYSHELF_SIM_FW).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "core/install.h"
#include "install/aes.hpp"
#include "install/cnmt.hpp"
#include "kit/core/sim_env.h"
#include "kit/core/sysinfo.h"
#include "util/paths.hpp"

namespace
{
const Result FAIL = NXM_RESULT(80);   // any failure of the simulated console

std::map<std::string, std::vector<uint8_t>> s_meta_pending;   // set or removed (empty), not yet committed
std::map<std::string, bool>                 s_meta_removed;
bool                                        s_open = false;

std::string root() { return paths::data_dir() + "/sim_console"; }
std::string hex(const u8 id[16])
{
    install::ContentId c;
    std::memcpy(c.data(), id, 16);
    return install::content_id_hex(c);
}
std::string placeholder(const u8 id[16]) { return root() + "/placeholders/" + hex(id); }
std::string content(const u8 id[16]) { return root() + "/contents/" + hex(id) + ".nca"; }
std::string meta_name(const CoreMetaKey* k)
{
    char b[64];
    std::snprintf(b, sizeof b, "%016llx-%u", (unsigned long long)k->id, (unsigned)k->version);
    return b;
}

bool exists(const std::string& p)
{
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
}

void mkdirs()
{
    ::mkdir(paths::data_dir().c_str(), 0755);
    for (const char* d : {"", "/placeholders", "/contents", "/meta", "/tickets"})
        ::mkdir((root() + d).c_str(), 0755);
}

bool read_file(const std::string& p, std::vector<uint8_t>* out)
{
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    out->clear();
    uint8_t buf[65536];
    size_t  n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out->insert(out->end(), buf, buf + n);
    std::fclose(f);
    return true;
}

bool write_file(const std::string& p, const void* data, size_t size)
{
    FILE* f = std::fopen(p.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(data, 1, size, f) == size;
    return std::fclose(f) == 0 && ok;
}
}   // namespace

extern "C" {

Result core_install_open(CoreStorage)
{
    mkdirs();
    s_open = true;
    return 0;
}

void core_install_close(void)
{
    s_open = false;
    s_meta_pending.clear();
    s_meta_removed.clear();
}

Result core_content_has(const u8 id[16], bool* out)
{
    *out = exists(content(id));
    return 0;
}

Result core_placeholder_create(const u8 id[16], u64)
{
    if (exists(placeholder(id))) return FAIL;
    return write_file(placeholder(id), "", 0) ? 0 : FAIL;
}

Result core_placeholder_write(const u8 id[16], u64 offset, const void* data, size_t size)
{
    FILE* f = std::fopen(placeholder(id).c_str(), "r+b");
    if (!f) return FAIL;
    const bool ok = std::fseek(f, (long)offset, SEEK_SET) == 0 && std::fwrite(data, 1, size, f) == size;
    return std::fclose(f) == 0 && ok ? 0 : FAIL;
}

Result core_placeholder_register(const u8 id[16])
{
    if (exists(content(id))) return FAIL;
    return std::rename(placeholder(id).c_str(), content(id).c_str()) == 0 ? 0 : FAIL;
}

void core_placeholder_delete(const u8 id[16]) { std::remove(placeholder(id).c_str()); }

Result core_placeholder_has(const u8 id[16], bool* out)
{
    *out = exists(placeholder(id));
    return 0;
}

Result core_placeholder_list(u8 (*out)[16], int max, int* count)
{
    *count = 0;
    DIR* dir = ::opendir((root() + "/placeholders").c_str());
    if (!dir) return 0;
    while (const dirent* e = ::readdir(dir)) {
        const auto id = install::content_id_from_hex(e->d_name);
        if (*count >= max || !id) continue;
        std::memcpy(out[(*count)++], id->data(), 16);
    }
    ::closedir(dir);
    return 0;
}
void core_content_delete(const u8 id[16]) { std::remove(content(id).c_str()); }

Result core_free_space(u64* out)
{
    const char* mb = sim_env("PLAYSHELF_SIM_FREE_MB");
    *out = (mb ? std::strtoull(mb, nullptr, 10) : 32768ull) << 20;
    return 0;
}

Result core_meta_get(const CoreMetaKey* key, void* buf, u64 buf_size, u64* size, bool* found)
{
    std::vector<uint8_t> rec;
    *found = read_file(root() + "/meta/" + meta_name(key) + ".bin", &rec);
    *size = *found ? rec.size() : 0;
    if (*found && buf) std::memcpy(buf, rec.data(), (size_t)(buf_size < rec.size() ? buf_size : rec.size()));
    return 0;
}

Result core_meta_set(const CoreMetaKey* key, const void* record, u64 size)
{
    s_meta_pending[meta_name(key)].assign((const uint8_t*)record, (const uint8_t*)record + size);
    s_meta_removed.erase(meta_name(key));
    return 0;
}

Result core_installed_list(CoreMetaKey* out, int max, int* count)
{
    // meta/<id>-<version>.bin, as core_meta_commit() writes them; the type
    // told by the id, as a real console's database keeps it.
    *count = 0;
    DIR* dir = ::opendir((root() + "/meta").c_str());
    if (!dir) return 0;
    while (const dirent* e = ::readdir(dir)) {
        // "<16 hex digits>-<version>.bin"
        const char* name = e->d_name;
        char*       end = nullptr;
        if (*count >= max || std::strlen(name) < 22 || name[16] != '-') continue;
        const unsigned long long id = std::strtoull(name, &end, 16);
        if (end != name + 16) continue;
        const unsigned long version = std::strtoul(name + 17, &end, 10);
        if (end == name + 17 || std::strcmp(end, ".bin") != 0) continue;
        CoreMetaKey& k = out[(*count)++];
        k = CoreMetaKey{};
        k.id = id;
        k.version = (u32)version;
        k.type = (id & 0x1000) ? 0x82 : (id & 0x800) ? 0x81 : 0x80;
        k.install_type = 0;
    }
    ::closedir(dir);
    return 0;
}

Result core_meta_remove(const CoreMetaKey* key)
{
    s_meta_pending.erase(meta_name(key));
    s_meta_removed[meta_name(key)] = true;
    return 0;
}

Result core_meta_commit(void)
{
    for (const auto& kv : s_meta_pending)
        if (!write_file(root() + "/meta/" + kv.first + ".bin", kv.second.data(), kv.second.size())) return FAIL;
    for (const auto& kv : s_meta_removed)
        std::remove((root() + "/meta/" + kv.first + ".bin").c_str());
    s_meta_pending.clear();
    s_meta_removed.clear();
    return 0;
}

Result core_push_application_record(u64 application_id, const CoreMetaKey* key)
{
    FILE* f = std::fopen((root() + "/records.log").c_str(), "a");
    if (!f) return FAIL;
    std::fprintf(f, "%016llx %s type=0x%02x\n", (unsigned long long)application_id, meta_name(key).c_str(), key->type);
    return std::fclose(f) == 0 ? 0 : FAIL;
}

Result core_import_ticket(const void* ticket, size_t ticket_size, const void* cert, size_t cert_size)
{
    int n = 0;
    while (exists(root() + "/tickets/" + std::to_string(n) + ".tik"))
        n++;
    const std::string base = root() + "/tickets/" + std::to_string(n);
    return write_file(base + ".tik", ticket, ticket_size) && write_file(base + ".cert", cert, cert_size) ? 0 : FAIL;
}

Result core_header_key(u8 out[32])
{
    for (int i = 0; i < 32; i++)
        out[i] = (u8)(i + 1);
    return 0;
}

Result core_unwrap_key(u8, u8 index, const u8 in[16], u8 out[16])
{
    static const u8 KAEK[16] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
    if (index != 0) return FAIL;
    std::memcpy(out, in, 16);
    install::aes128_ecb_decrypt(KAEK, out, 16);
    return 0;
}

u32 core_system_version(void)
{
    SysInfo info;
    sysinfo_get(&info);
    return info.hos_version;
}

}   // extern "C"
