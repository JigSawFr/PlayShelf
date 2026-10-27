// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/play.h"

#include <string.h>
#include <switch.h>

bool core_play_open(void)
{
    return R_SUCCEEDED(pdmqryInitialize());
}

void core_play_close(void)
{
    pdmqryExit();
}

bool core_play_stats(u64 app_id, CorePlay *out)
{
    memset(out, 0, sizeof(*out));
    PdmPlayStatistics st;
    memset(&st, 0, sizeof(st));
    if (R_FAILED(pdmqryQueryPlayStatisticsByApplicationId(app_id, false, &st))) return false;
    if (st.playtime == 0 && st.total_launches == 0) return false;
    out->total_s = st.playtime / 1000000000ULL;
    out->launches = st.total_launches;
    out->last_played = st.last_timestamp_user;
    return true;
}
