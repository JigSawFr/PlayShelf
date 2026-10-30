// sim_demo::start(): the demo's answers in place of the network on the
// desktop, and on a first start its server, its first games on the
// simulated console, its wishlist and its title database
// (source/sim/sim_demo.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <dirent.h>
#include <functional>
#include <string>
#include <utility>
#include <sys/stat.h>

#include "core/sim_env.h"
#include "library/servers.hpp"
#include "sim/sim_demo.hpp"
#include "titledb/store.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

namespace sim_demo
{
namespace
{
// The demo's library server, as the user would set it up.
library::Server server()
{
    library::Server s;
    s.id = "de0de0de0de0de00";
    s.name = "Demo shelf";
    s.wan_url = DEMO_ORIGIN;
    return s;
}

// sys-patch on the simulated SD card, as a console that installs games has
// (source/kit/util/patches.cpp reads it).
void sys_patch()
{
    const std::string sd = paths::sd_root(), contents = sd + "/atmosphere/contents/420000000000000B",
                      log = sd + "/config/sys-patch";
    for (const std::string& d : { sd + "/atmosphere", sd + "/atmosphere/contents", contents, contents + "/flags",
                                  sd + "/config", log })
        ::mkdir(d.c_str(), 0755);
    (void)paths::atomic_write(contents + "/exefs.nsp", "");
    (void)paths::atomic_write(contents + "/flags/boot2.flag", "");
    (void)paths::atomic_write(log + "/log.ini", "[stats]\nversion=1.5.7\nfw_version=23.0.1\nis_emummc=0\n"
                                                "[fs]\nnoncasigchk_new=Patched (sys-patch)\n"
                                                "[ldr]\nnoacidsigchk=Patched (sys-patch)\n[es]\nes2=Patched (sys-patch)\n");
}

// The demo's first games: their content meta records (meta/), their names
// and icons (control/); the data folder's own files (data/: the wishlist).
void seed_console()
{
    const std::string console = paths::data_dir() + "/sim_console";
    ::mkdir(console.c_str(), 0755);
    const std::pair<const char*, std::string> parts[] = { { "/meta", console + "/meta" },
                                                          { "/control", console + "/control" },
                                                          { "/data", paths::data_dir() } };
    for (const auto& [part, to] : parts) {
        const std::string from = dir() + "/console" + part;
        ::mkdir(to.c_str(), 0755);
        DIR* d = ::opendir(from.c_str());
        if (!d) continue;
        while (const dirent* e = ::readdir(d)) {
            const std::string name = e->d_name;
            std::string       bytes;
            if (name[0] != '.' && paths::read_file(from + "/" + name, bytes)) (void)paths::atomic_write(to + "/" + name, bytes);
        }
        ::closedir(d);
    }
    // The title database's source the demo answers (TITLEDB_URL, one of the
    // default sources), fetched as Preferences › Title database would: the
    // coming games' names and dates, from the first screen.
    titledb::Net net;
    net.save = [](const std::string& url, const std::string& path, uint64_t max,
                  const std::function<void(uint64_t, uint64_t)>& progress, std::string* why) {
        return http::download(url, path, why, (size_t)max, 30, progress, 0, nullptr);
    };
    std::string why;
    if (titledb::update(TITLEDB_URL, titledb::source_dir(paths::data_dir() + "/titledb", TITLEDB_URL), net,
                        [](uint64_t, uint64_t) {}, &why) == titledb::Outcome::Failed)
        std::fprintf(stderr, "demo: no title database (%s)\n", why.c_str());
}
}   // namespace

void start()
{
    if (dir().empty()) return;
#ifndef __EMSCRIPTEN__
    static const http::StandIn answers{ serves,  get,           post,    redirect, download, range,
                                        resource_size, request, call, peer_certificate };
    http::set_stand_in(&answers);
#endif
    if (sim_env_flag("PLAYSHELF_SIM_NO_DEMO")) return;
#ifndef __EMSCRIPTEN__
    // (The web page sets it itself, unless its URL says signed=.)
    if (!sim_env("PLAYSHELF_SIM_SIGNED")) sim_env_set("PLAYSHELF_SIM_SIGNED", "1");
#endif
    struct stat st;
    if (::stat(library::servers_file().c_str(), &st) == 0) return;
    std::string why;
    ::mkdir(paths::data_dir().c_str(), 0755);
    if (!library::save_servers({ server() }, &why)) std::fprintf(stderr, "demo: no server (%s)\n", why.c_str());
    sys_patch();
    seed_console();
}

}   // namespace sim_demo
