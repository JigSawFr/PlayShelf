// core/games — what the console says of an installed game: its name, in the
// console's language, and its icon (a JPEG), from ns' control data (its
// cache first, as the HOME menu reads it, then the storage).
//
// On the Switch this is libnx (games.c, through the kit's nacp_read); the
// desktop build reads a folder (sim/sim_games.cpp). Also a game deleted.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "kit/core/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The icon's largest size (NsApplicationControlData's).
#define CORE_GAME_ICON_MAX 0x20000

// `app_id`'s name into `name` (empty when it has none), the version it
// shows ("1.2.0", the control data's DisplayVersion: an update's once
// installed) into `version`, and its icon into `icon` (CORE_GAME_ICON_MAX
// bytes; *icon_size 0 when it has none). False when the console has no
// control data for it.
bool core_game_control(u64 app_id, char *name, size_t name_size, char *version, size_t version_size, void *icon,
                       size_t *icon_size);

// Deletes the game from the console: its base game, its update, its
// add-ons and its record (ns' DeleteApplicationCompletely). Its saves stay.
Result core_game_delete(u64 app_id);

#ifdef __cplusplus
}
#endif
