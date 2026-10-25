// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/focus_log.hpp"

#include <borealis.hpp>

#include "core/sim_env.h"

namespace focus_log
{
void install()
{
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    if (!sim_env_flag("PLAYSHELF_SIM_FOCUS_LOG")) return;
    brls::Application::getGlobalFocusChangeEvent()->subscribe([](brls::View* view) {
        // A list's line (a sheet's too), else a dialog's button.
        if (auto* cell = dynamic_cast<brls::DetailCell*>(view)) brls::Logger::info("focus: {}", cell->title->getFullText());
        else if (auto* button = dynamic_cast<brls::Button*>(view)) brls::Logger::info("focus: {}", button->getText());
    });
#endif
}
}   // namespace focus_log
