// web_demo::seed_console(): the web demo's first games on the simulated
// console (source/sim/web_demo.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <dirent.h>
#include <string>
#include <sys/stat.h>

#include "sim/web_demo.hpp"
#include "util/paths.hpp"

namespace web_demo
{

void seed_console()
{
    const std::string console = paths::data_dir() + "/sim_console", request = console + "/seed_demo";
    struct stat       st;
    if (::stat(request.c_str(), &st) != 0) return;
    // Their content meta records (meta/), their names and icons (control/).
    for (const char* part : { "/meta", "/control" }) {
        const std::string from = std::string(DEMO_DIR) + "/console" + part, to = console + part;
        ::mkdir(to.c_str(), 0755);
        DIR* dir = ::opendir(from.c_str());
        if (!dir) continue;
        while (const dirent* e = ::readdir(dir)) {
            const std::string name = e->d_name;
            std::string       bytes;
            if (name[0] != '.' && paths::read_file(from + "/" + name, bytes)) paths::atomic_write(to + "/" + name, bytes);
        }
        ::closedir(dir);
    }
    std::remove(request.c_str());
}

}   // namespace web_demo
