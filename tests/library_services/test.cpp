// library/services.cpp: the cheat files on the SD card: a build id's form,
// the file's path, the builds a game has cheats installed for.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "fs.hpp"
#include "library/services.hpp"
#include "temp_dir.hpp"
#include "util/paths.hpp"

using namespace library;

int main()
{
    CHECK(normal_build_id("0a1b2C") == "0A1B2C");
    CHECK(normal_build_id("").empty() && normal_build_id("xyz").empty() && normal_build_id(std::string(65, 'a')).empty());
    CHECK(normal_build_id(std::string(64, 'f')) == std::string(64, 'F'));

    CHECK(cheat_path("/sd/", 0x0100AAAA00010000ull, "ab12") == "/sd/atmosphere/contents/0100AAAA00010000/cheats/AB12.txt");
    CHECK(cheat_path("/sd", 0, "ab12").empty() && cheat_path("/sd", 1, "../x").empty());

    TempDir tmp("library_services");
    const uint64_t id = 0x0100AAAA00010000ull;
    CHECK(installed_cheats(tmp.path(), id).empty());
    for (const char* build : {"BBBB", "aaaa"}) {
        const std::string path = cheat_path(tmp.path(), id, build);
        CHECK(paths::ensure_dir(path.substr(0, path.find_last_of('/'))));
        fs::write(path, "[cheat]\n");
    }
    fs::write(tmp.path() + "/atmosphere/contents/0100AAAA00010000/cheats/readme.txt", "not a build");
    const auto have = installed_cheats(tmp.path(), id);
    CHECK(have == std::vector<std::string>({ "AAAA", "BBBB" }));
    CHECK_DONE("library services: build ids, cheat paths and the cheats installed passed");
}
