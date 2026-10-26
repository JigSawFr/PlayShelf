// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/games.h"

#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "kit/core/nacp.h"

// NacpStruct's display_version: 0x10 bytes at 0x3060.
#define NACP_DISPLAY_VERSION      0x3060
#define NACP_DISPLAY_VERSION_SIZE 0x10

bool core_game_control(u64 app_id, char *name, size_t name_size, char *version, size_t version_size, void *icon,
                       size_t *icon_size)
{
    if (name_size) name[0] = 0;
    if (version_size) version[0] = 0;
    *icon_size = 0;
    if (R_FAILED(nsInitialize())) return false;
    NsApplicationControlData *cd = (NsApplicationControlData *)malloc(sizeof(*cd));
    u64  size = 0;
    bool ok = cd && nacp_read_control(app_id, cd, false, &size);
    if (ok) {
        // The console's language: read once.
        static u32  title_index;
        static bool language_read;
        if (!language_read) {
            nacp_console_language(&title_index);
            language_read = true;
        }
        nacp_name(&cd->nacp, sizeof(cd->nacp), title_index, name, name_size);
        char shown[NACP_DISPLAY_VERSION_SIZE + 1] = {0};
        memcpy(shown, (const u8 *)&cd->nacp + NACP_DISPLAY_VERSION, NACP_DISPLAY_VERSION_SIZE);
        if (nacp_utf8_valid(shown, strlen(shown))) nacp_copy_utf8(version, version_size, shown);
        if (size > sizeof(cd->nacp)) {
            *icon_size = (size_t)(size - sizeof(cd->nacp));
            if (*icon_size > CORE_GAME_ICON_MAX) *icon_size = CORE_GAME_ICON_MAX;
            memcpy(icon, cd->icon, *icon_size);
        }
    }
    free(cd);
    nsExit();
    return ok;
}
