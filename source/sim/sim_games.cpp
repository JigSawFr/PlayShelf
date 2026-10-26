// Simulated core/games.h: an installed game's control data as files of the
// simulated console (playshelf_data/sim_console/control/):
//   <title id>.txt   its name, and on a second line the version it shows
//                    (16 hex digits, the base game's)
//   <title id>.jpg   its icon
// A game without them has no control data: the Library tab names it from
// a catalogue, else by its title id. A game deleted: its content meta
// records (meta/) gone, its base game's, its update's and its add-ons'.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <cstring>
#include <string>

#include "core/games.h"
#include "library/catalog.hpp"
#include "util/paths.hpp"

bool core_game_control(u64 app_id, char* name, size_t name_size, char* version, size_t version_size, void* icon,
                       size_t* icon_size)
{
    if (name_size) name[0] = 0;
    if (version_size) version[0] = 0;
    *icon_size = 0;
    char id[17];
    std::snprintf(id, sizeof id, "%016llx", (unsigned long long)app_id);
    const std::string base = paths::data_dir() + "/sim_console/control/" + id;
    std::string text;
    if (!paths::read_file(base + ".txt", text)) return false;
    const size_t line = text.find('\n');
    std::string  shown = line == std::string::npos ? "" : text.substr(line + 1);
    text = text.substr(0, line);
    for (std::string* s : { &text, &shown })
        while (!s->empty() && (s->back() == '\n' || s->back() == '\r')) s->pop_back();
    std::snprintf(name, name_size, "%s", text.c_str());
    std::snprintf(version, version_size, "%s", shown.c_str());
    std::string jpg;
    if (paths::read_file(base + ".jpg", jpg) && jpg.size() <= CORE_GAME_ICON_MAX) {
        std::memcpy(icon, jpg.data(), jpg.size());
        *icon_size = jpg.size();
    }
    return true;
}

Result core_game_delete(u64 app_id)
{
    const std::string dir = paths::data_dir() + "/sim_console/meta";
    DIR*              d = ::opendir(dir.c_str());
    if (!d) return 0;
    while (const dirent* e = ::readdir(d)) {
        const std::string name = e->d_name;   // <title id>-<version>.bin
        if (name.size() < 17 || name[16] != '-') continue;
        const u64 id = std::strtoull(name.substr(0, 16).c_str(), nullptr, 16);
        if (library::base_of(id) == app_id) std::remove((dir + "/" + name).c_str());
    }
    ::closedir(d);
    return 0;
}
