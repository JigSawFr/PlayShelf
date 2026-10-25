// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/effects.hpp"

#include <string>

#include "core/sim_env.h"
#include "ui/text_size.hpp"
#include "util/config.hpp"

namespace effects
{

bool in_hand()
{
#ifdef __SWITCH__
    return !text_size::docked();
#else
    return sim_env_flag("PLAYSHELF_SIM_BATTERY");
#endif
}

bool light()
{
    return config::get().effects == "auto" && in_hand();
}

bool reduced()
{
    return config::get().effects == "reduced";
}

bool motion()
{
    // The smoke test's screenshots compare frames: nothing may depend on when
    // they are taken (the same flag holds the focus highlight still).
    static const bool still = sim_env_flag("PLAYSHELF_SIM_STILL_FOCUS");
    if (still) return false;
    const std::string& setting = config::get().effects;
    if (setting == "reduced") return false;
    return setting != "auto" || !in_hand();
}

}   // namespace effects
