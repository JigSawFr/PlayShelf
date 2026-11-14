// safety_probe — what the console says of itself for safety_logic: emuMMC
// (Atmosphere's configuration item, through spl) and the names of Nintendo's
// servers (DNS). It takes a moment (name resolution): call it off the UI
// thread. Everything is Unknown away from a console.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "action/safety_logic.hpp"

namespace safety_probe
{

safety_logic::Probe run();

}   // namespace safety_probe
