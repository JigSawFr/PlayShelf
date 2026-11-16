// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/agent_setup.hpp"

#include <aurore.hpp>
#include <string>

#include "action/agent_manage_flow.hpp"
#include "action/link_flow.hpp"
#include "core/sim_env.h"

namespace agent_setup
{

namespace
{
// The romfs folder holding the agent (cmake/bundle_sysmodules.cmake); the
// desktop build reads PLAYSHELF_SIM_BUNDLED instead, to try an update.
std::string bundle_dir()
{
#ifndef __SWITCH__
    if (const char* sim = sim_env("PLAYSHELF_SIM_BUNDLED")) return sim;
#endif
    return AURORE_ASSET("sysmodules");
}
}   // namespace

void install()
{
    auto& h = agent_manage_flow::hooks();
    h.bundle_dir = bundle_dir;
    h.hold = [](bool on) { link_flow::hold(on); };
    // The turns of link_flow look for the agent again by themselves.
    h.link_back = []() { link_flow::hold(false); };
}

}   // namespace agent_setup
