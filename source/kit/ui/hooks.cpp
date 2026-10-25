// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/hooks.hpp"

namespace ui::hooks
{

Hooks& get()
{
    static Hooks hooks;
    return hooks;
}

}   // namespace ui::hooks
