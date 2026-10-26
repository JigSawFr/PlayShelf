// ui — the read-only mode's refusals (Aurore's Cadran has the rest).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/ui/hooks.hpp>

#include "app.hpp"

#include <algorithm>
#include <yoga/Yoga.h>

using namespace brls::literals;

namespace ui
{

bool refuse_read_only()
{
    if (!app::read_only()) return false;
    refuse(NXM_RC_READ_ONLY);
    return true;
}

void guard_switch(brls::BooleanCell* cell)
{
    cell->registerClickAction([cell](brls::View*) {
        if (refuse_read_only()) return true;
        cell->setOn(!cell->isOn());
        cell->getEvent()->fire(cell->isOn());
        return true;
    });
}

}   // namespace ui
