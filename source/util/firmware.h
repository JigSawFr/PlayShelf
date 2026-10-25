// firmware — the firmware PlayShelf was verified on. compat.json publishes
// it with every release (tools/gen_compat.py), and the update check reads it
// to say whether a newer release supports this console's firmware
// (util/update.hpp). Raise it only after a check on a console; the first
// release checks this one there first.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "core/nx_types.h"

#define APP_FW_TESTED_MAX MAKEHOSVERSION(23, 0, 1)
