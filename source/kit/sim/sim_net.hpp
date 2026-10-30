// sim_net — the desktop build's simulated network, shared by the *_net.cpp
// files: an <APP_ENV>_SIM_* variable names a folder whose files answer the
// requests, or "offline" (each file says which variable it reads, and what
// it answers without one). Desktop only: source/sim/ is not built for the
// console, so it is included under #ifndef __SWITCH__.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace sim_net
{

// The value of the environment variable `name`; nullptr when it is unset or
// empty (the real network, or none, as the caller decides).
const char* value(const char* name);

// No connection: false, and *error (when set) "simulated: no connection".
bool offline(std::string* error);

// A GET answered by the file `name` of the folder `folder`. No folder,
// "offline" or no such file: offline().
[[nodiscard]] bool get(const char* folder, const std::string& name, std::string* body, std::string* error);

}   // namespace sim_net
