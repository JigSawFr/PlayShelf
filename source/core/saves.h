// core/saves — the console's users and their games' saves: who the users
// are (their nickname), which games each one has a save for, and a game's
// save mounted as a folder ("pssave:/") to be read whole or written again
// (committed as it goes: a save's journal holds so much at a time).
//
// On the Switch this is libnx (saves.c: account, fs, fsdev); the desktop
// build keeps the saves as folders of the simulated console
// (sim/sim_saves.cpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "kit/core/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_SAVE_USERS_MAX 12
#define CORE_SAVE_NAME_MAX  0x21   // a nickname, 32 bytes and its end

typedef struct
{
    u64  uid[2];
    char name[CORE_SAVE_NAME_MAX];
} CoreSaveUser;

// The console's users, at most `max`; -1 when they cannot be read.
int core_save_users(CoreSaveUser *out, int max);

// The games `user` has a save of, at most `max`; -1 when they cannot be read.
int core_save_titles(const CoreSaveUser *user, u64 *out, int max);

// `app_id`'s save of `user` mounted: its folder's path into `root`
// ("pssave:/"). One at a time; `writable` false mounts it read-only.
Result core_save_mount(const CoreSaveUser *user, u64 app_id, bool writable, char *root, size_t root_size);
// What was written to the mounted save kept (its journal emptied).
Result core_save_commit(void);
void   core_save_unmount(void);

#ifdef __cplusplus
}
#endif
