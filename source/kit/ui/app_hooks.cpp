// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/app_hooks.hpp"

namespace ui::app_hooks
{

Hooks& get()
{
    static Hooks hooks;
    return hooks;
}

}   // namespace ui::app_hooks
