// web_demo::seed_console(): the web demo's first games on the simulated
// console, its wishlist and its title database (source/sim/web_demo.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <dirent.h>
#include <functional>
#include <string>
#include <utility>
#include <sys/stat.h>

#include "sim/web_demo.hpp"
#include "titledb/store.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

namespace web_demo
{

void seed_console()
{
    const std::string console = paths::data_dir() + "/sim_console", request = console + "/seed_demo";
    struct stat       st;
    if (::stat(request.c_str(), &st) != 0) return;
    // Their content meta records (meta/), their names and icons (control/);
    // the data folder's own files (data/: the wishlist).
    const std::pair<const char*, std::string> parts[] = { { "/meta", console + "/meta" },
                                                          { "/control", console + "/control" },
                                                          { "/data", paths::data_dir() } };
    for (const auto& [part, to] : parts) {
        const std::string from = std::string(DEMO_DIR) + "/console" + part;
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
        std::fprintf(stderr, "web demo: no title database (%s)\n", why.c_str());
    std::remove(request.c_str());
}

}   // namespace web_demo
