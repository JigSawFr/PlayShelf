// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/boreale_apps_logic.hpp"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include "bl_app.h"
#include "bl_updates.h"
#include "action/shared_bell_logic.hpp"

namespace boreale_apps_logic
{

namespace
{
bool read_file(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

std::string under(const std::string& sd_root, const std::string& path)
{
    return sd_root + (sd_root.empty() || sd_root.back() != '/' ? "/" : "") + path;
}
}   // namespace

std::vector<App> installed(const std::string& sd_root)
{
    std::vector<App> out;
    DIR* dir = opendir(under(sd_root, BL_APPS_DIR).c_str());
    if (!dir) return out;
    while (const dirent* ent = readdir(dir)) {
        char module[BL_APP_MODULE_MAX];
        if (!bl_app_module_of(ent->d_name, module, sizeof(module))) continue;
        std::string text;
        if (!read_file(under(sd_root, BL_APPS_DIR "/") + ent->d_name, text)) continue;
        BlApp a;
        bl_app_defaults(&a);
        bl_app_parse(&a, text.data(), text.size());
        // Deleted: its .nro gone. One started from elsewhere (nxlink) does
        // not know it: kept, not launched.
        const char* nro = a.nro[0] == '/' ? a.nro + 1 : a.nro;
        struct stat st;
        if (nro[0] && stat(under(sd_root, nro).c_str(), &st) != 0) continue;
        App app;
        app.module = module;
        app.name = a.name[0] ? a.name : module;
        app.version = a.version;
        app.repo = a.repo;
        if (nro[0]) app.nro = std::string("/") + nro;
        app.self = !std::strcmp(module, shared_bell_logic::MODULE);
        out.push_back(std::move(app));
    }
    closedir(dir);
    std::sort(out.begin(), out.end(), [](const App& a, const App& b) {
        if (a.self != b.self) return a.self;
        return a.name < b.name;
    });
    return out;
}

std::string updates_file(const std::string& sd_root)
{
    return under(sd_root, BL_UPDATES_FILE);
}

bool updates_due(const std::string& text, int64_t now)
{
    BlUpdates u;
    bl_updates_defaults(&u);
    bl_updates_parse(&u, text.data(), text.size());
    return bl_updates_due(&u, now);
}

std::string updates_text(const std::string& text, const std::vector<std::pair<std::string, std::string>>& latest,
                         int64_t now)
{
    BlUpdates u;
    bl_updates_defaults(&u);
    bl_updates_parse(&u, text.data(), text.size());
    if (!bl_updates_may_write(&u)) return "";
    for (const auto& [module, version] : latest) bl_updates_set(&u, module.c_str(), version.c_str());
    u.checked = now;
    std::string out(4096, '\0');
    out.resize(bl_updates_write(&u, &out[0], out.size()));
    return out;
}

}   // namespace boreale_apps_logic
