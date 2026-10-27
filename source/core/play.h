// core/play — how much each game has been played on the console, every
// account together: its all-time play time, how many times it was started
// and when it was last played, from the play-data service (pdm:qry), as
// PlayGuard reads them. Read-only.
//
// On the Switch this is libnx (play.c); the desktop build makes them up
// from the title id (sim/sim_play.cpp). PLAYSHELF_SIM_NO_PLAY=1: none.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "kit/core/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    u64 total_s;       // all-time play time, seconds
    u32 launches;
    u64 last_played;   // POSIX (the console's clock), 0 when unknown
} CorePlay;

// The service opened for a batch of core_play_stats(), then closed. False
// when it cannot be opened (the statistics are then left out).
bool core_play_open(void);
void core_play_close(void);

// `app_id`'s statistics; false when it has none (never played).
bool core_play_stats(u64 app_id, CorePlay *out);

#ifdef __cplusplus
}
#endif
