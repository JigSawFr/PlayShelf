// PlayShelf — a game's control data, the console side (see nacp.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "nacp.h"

bool nacp_read_control(u64 app_id, void *control, bool icon, u64 *size)
{
    NsApplicationControlData *cd = (NsApplicationControlData *)control;
    static const NsApplicationControlSource sources[] = { NsApplicationControlSource_CacheOnly,
                                                          NsApplicationControlSource_Storage };
    for (size_t k = 0; k < sizeof(sources) / sizeof(sources[0]); k++) {
        *size = 0;
        if (R_SUCCEEDED(nsGetApplicationControlData(sources[k], app_id, cd, sizeof(*cd), size)) &&
            *size >= sizeof(cd->nacp) + (icon ? 1 : 0) && *size <= sizeof(*cd))
            return true;
    }
    return false;
}

// The title entry of each console language (SetLanguage), as libnx's
// nacpGetLanguageEntry() maps them.
static const u8 NACP_LANGUAGE[SetLanguage_Total] = {
    [SetLanguage_JA] = 2,      [SetLanguage_ENUS] = 0,   [SetLanguage_FR] = 3,     [SetLanguage_DE] = 4,
    [SetLanguage_IT] = 7,      [SetLanguage_ES] = 6,     [SetLanguage_ZHCN] = 14,  [SetLanguage_KO] = 12,
    [SetLanguage_NL] = 8,      [SetLanguage_PT] = 10,    [SetLanguage_RU] = 11,    [SetLanguage_ZHTW] = 13,
    [SetLanguage_ENGB] = 1,    [SetLanguage_FRCA] = 9,   [SetLanguage_ES419] = 5,  [SetLanguage_ZHHANS] = 14,
    [SetLanguage_ZHHANT] = 13, [SetLanguage_PTBR] = 15,
};

u8 nacp_console_language(u32 *title_index)
{
    *title_index = 0;
    u64 code = 0;
    SetLanguage lang = SetLanguage_ENUS;
    if (R_FAILED(setInitialize())) return 0;
    const bool ok = R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &lang)) &&
                    (u32)lang < SetLanguage_Total;
    setExit();
    if (!ok) return 0;
    *title_index = NACP_LANGUAGE[lang];
    return (u8)(lang + 1);
}
