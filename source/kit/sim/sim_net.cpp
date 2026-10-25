// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sim/sim_net.hpp"

#include <cstdlib>
#include <cstring>

#include "core/sim_env.h"
#include "util/paths.hpp"

namespace sim_net
{

const char* value(const char* name)
{
    const char* v = sim_env(name);
    return v && *v ? v : nullptr;
}

bool offline(std::string* error)
{
    if (error) *error = "simulated: no connection";
    return false;
}

bool get(const char* folder, const std::string& name, std::string* body, std::string* error)
{
    // A file of the folder itself: `name` can come from an answer (a
    // version read in a page), and "../x" or "a/b" would leave it.
    const bool plain = !name.empty() && name[0] != '.' && name.find_first_of("/\\:") == std::string::npos;
    if (!folder || std::strcmp(folder, "offline") == 0 || !plain
        || !paths::read_file(std::string(folder) + "/" + name, *body))
        return offline(error);
    return true;
}

}   // namespace sim_net
