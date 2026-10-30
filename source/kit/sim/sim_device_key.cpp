// Simulated core/device_key.h: a random key made once and kept in the data
// folder (device.key), so the desktop build seals its
// secrets as the console does, for this PC alone.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <random>
#include <string>
#include <sys/stat.h>

#include "../core/device_key.h"
#include "util/paths.hpp"

extern "C" Result device_key(u8 out[16])
{
    const std::string path = paths::data_dir() + "/device.key";
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        const size_t n = std::fread(out, 1, 16, f);
        std::fclose(f);
        if (n == 16) return 0;
    }
    ::mkdir(paths::data_dir().c_str(), 0755);
    std::random_device rd;
    for (int i = 0; i < 16; i++)
        out[i] = (u8)rd();
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return NXM_RESULT(81);
    const bool ok = std::fwrite(out, 1, 16, f) == 16;
    return std::fclose(f) == 0 && ok ? 0 : NXM_RESULT(81);
}
