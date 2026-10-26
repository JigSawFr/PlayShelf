// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "titledb/nlib.hpp"

#include <cinttypes>
#include <cstdio>

namespace titledb
{

std::string nlib_icon_url(uint64_t base_id, int size)
{
    // A base game ends in 000 with bit 12 clear (library::kind_of).
    if (!base_id || (base_id & 0x1fff) || size < 30 || size > 4096) return "";
    char url[64];
    std::snprintf(url, sizeof url, "https://api.nlib.cc/nx/%016" PRIX64 "/icon/%d", base_id, size);
    return url;
}

}   // namespace titledb
