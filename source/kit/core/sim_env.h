// The desktop and web simulators' <APP_ENV>_SIM_* variables (app_id.h) (CONTRIBUTING.md,
// web/README.md): the environment's, unless the web page changed one while
// the app runs. Read through these functions, never getenv(): the page's
// changes do not touch the process environment, which other threads read
// without a lock. On the console, there is none: every variable is unset.
//
// A flag or a number is read with sim_env_flag() / sim_env_int(), so every
// variable takes the same values (what the web page writes, too); sim_env()
// is for the ones that hold a text.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __SWITCH__
static inline const char *sim_env(const char *name)
{
    (void)name;
    return 0;
}
static inline bool sim_env_flag(const char *name)
{
    (void)name;
    return false;
}
static inline long long sim_env_int(const char *name, long long def, long long min, long long max)
{
    (void)name; (void)min; (void)max;
    return def;
}
#else
// The variable's value, NULL when unset. The string stays valid as long as
// the program runs.
const char *sim_env(const char *name);
// On: set to anything but "", "0", "false", "off" or "no" (in any case), the
// values the web page's panel leaves out of its URL.
bool sim_env_flag(const char *name);
// A whole number (decimal, a sign allowed): `def` when the variable is unset,
// empty or not a number; else the number, brought within [min, max].
long long sim_env_int(const char *name, long long def, long long min, long long max);
// The web page's change (source/sim/web_page.cpp): `value` NULL or empty
// removes it, whatever the environment says.
void sim_env_set(const char *name, const char *value);
#endif

#ifdef __cplusplus
}
#endif
