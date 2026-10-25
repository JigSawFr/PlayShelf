// device_key — a 16-byte key of this console's own, the same at every
// start, that another console cannot derive: what util/secret_box seals
// secrets with. On the Switch, spl derives it from the console's
// device-unique key (a fixed source, GenerateAesKek with the device-unique
// option, then GenerateAesKey); on a PC (sim/sim_device_key.cpp) it is a
// random key kept in the data folder.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

Result device_key(u8 out[16]);

#ifdef __cplusplus
}
#endif
