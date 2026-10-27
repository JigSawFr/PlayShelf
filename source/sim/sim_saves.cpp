// Simulated core/saves.h: the users and their saves as folders of the
// simulated console (playshelf_data/sim_console/saves/):
//   users.txt              one user a line: 32 hex digits, a space, the nickname
//                          (none: one user, "Player")
//   <uid>/<title id>/      a game's save, its files as they are
// Mounted, a save is its folder; a commit keeps nothing more.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>

#include "core/saves.h"
#include "util/paths.hpp"

namespace
{
std::string saves_dir()
{
    return paths::data_dir() + "/sim_console/saves";
}

std::string uid_text(const CoreSaveUser* u)
{
    char text[33];
    std::snprintf(text, sizeof text, "%016llx%016llx", (unsigned long long)u->uid[0], (unsigned long long)u->uid[1]);
    return text;
}
}   // namespace

int core_save_users(CoreSaveUser* out, int max)
{
    std::string text;
    int         n = 0;
    if (paths::read_file(saves_dir() + "/users.txt", text)) {
        size_t at = 0;
        while (at < text.size() && n < max) {
            const size_t end = text.find('\n', at);
            std::string  line = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
            at = end == std::string::npos ? text.size() : end + 1;
            if (line.size() < 34 || line[32] != ' ') continue;
            CoreSaveUser& u = out[n++];
            std::memset(&u, 0, sizeof u);
            u.uid[0] = std::strtoull(line.substr(0, 16).c_str(), nullptr, 16);
            u.uid[1] = std::strtoull(line.substr(16, 16).c_str(), nullptr, 16);
            std::snprintf(u.name, sizeof u.name, "%s", line.substr(33).c_str());
        }
    }
    if (n == 0 && max > 0) {
        std::memset(&out[0], 0, sizeof out[0]);
        out[0].uid[0] = 0x0123456789abcdefull;
        out[0].uid[1] = 0xfedcba9876543210ull;
        std::snprintf(out[0].name, sizeof out[0].name, "Player");
        n = 1;
    }
    return n;
}

int core_save_titles(const CoreSaveUser* user, u64* out, int max)
{
    DIR* d = ::opendir((saves_dir() + "/" + uid_text(user)).c_str());
    if (!d) return 0;
    int n = 0;
    while (const dirent* e = ::readdir(d)) {
        if (n >= max || std::strlen(e->d_name) != 16) continue;
        char*     end = nullptr;
        const u64 id = std::strtoull(e->d_name, &end, 16);
        if (id && end && !*end) out[n++] = id;
    }
    ::closedir(d);
    return n;
}

Result core_save_mount(const CoreSaveUser* user, u64 app_id, bool writable, char* root, size_t root_size)
{
    char id[17];
    std::snprintf(id, sizeof id, "%016llx", (unsigned long long)app_id);
    const std::string dir = saves_dir() + "/" + uid_text(user) + "/" + id;
    struct stat       st;
    // A game the user never played has no save, to read or to write.
    (void)writable;
    if (::stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return NXM_RC_INVALID_ARGUMENT;
    std::snprintf(root, root_size, "%s/", dir.c_str());
    return 0;
}

Result core_save_commit(void)
{
    return 0;
}

void core_save_unmount(void) {}
