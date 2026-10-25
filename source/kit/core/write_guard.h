// write_guard — what the service layer checks before changing the console.
//
// The read-only switch: while it is on, every function that would change the
// console (source/core, and its simulated twins in source/kit/sim/) returns
// NXM_RC_READ_ONLY before opening any session. The UI hides its write
// controls too, but this is the check that cannot be skipped.
//
// The change check: a function the app may install, run before every
// change (a confirmation, a PIN); it refuses with NXM_RC_NOT_CONFIRMED. It
// runs before any session is opened, so it may show a system applet.
//
// The reveal check: the same, before something secret is shown. Unset, the
// change check stands in for it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void core_set_read_only(bool on);
bool core_read_only(void);

// NULL (the default): every change is allowed.
void core_set_change_check(bool (*check)(void));

// What every change calls first: NXM_RC_READ_ONLY, NXM_RC_NOT_CONFIRMED, or 0.
Result core_change_allowed(void);

// NULL (the default): the change check stands in for it.
void core_set_reveal_check(bool (*check)(void));

// What showing a secret calls first: the same results as core_change_allowed.
Result core_reveal_allowed(void);

#ifdef __cplusplus
}
#endif
