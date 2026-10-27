// Simulated core/play.h: play statistics made up from each game's title id,
// the same at every run: most games played, a few hours to a hundred, last
// played in the month before the simulated console's time. None with
// PLAYSHELF_SIM_NO_PLAY=1.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>
#include <ctime>

#include "core/play.h"
#include "kit/core/sim_env.h"

bool core_play_open(void)
{
    return !sim_env_flag("PLAYSHELF_SIM_NO_PLAY");
}

void core_play_close(void) {}

bool core_play_stats(u64 app_id, CorePlay* out)
{
    std::memset(out, 0, sizeof(*out));
    // A spread of the id's bits.
    u64 h = app_id * 0x9E3779B97F4A7C15ULL;
    h ^= h >> 29;
    if (h % 5 == 0) return false;   // one game in five never played
    out->total_s = (h % 120 + 1) * 3600 + (h >> 8) % 3600;
    out->launches = (u32)((h >> 16) % 200 + 1);
    out->last_played = (u64)std::time(nullptr) - (h >> 24) % (30 * 86400);
    return true;
}
