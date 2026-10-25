// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/text_size.hpp"

#include <borealis.hpp>
#include <cstdlib>
#include <string>
#include <vector>
#include <yoga/Yoga.h>

#include "core/sim_env.h"
#include "view/cadran_digits.hpp"
#include "ui/cadran_list.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace text_size
{
bool docked()
{
#ifdef __SWITCH__
    return appletGetOperationMode() == AppletOperationMode_Console;
#else
    return sim_env_flag("PLAYSHELF_SIM_DOCKED");
#endif
}

bool larger()
{
    const std::string& size = config::get().text_size;
    return size == "large" || (size == "auto" && docked());
}

namespace
{
// Each label's node made dirty (its text measured again at the new size),
// and the roots to lay out again: the screen's, and each detached one
// (a scrolling frame's content, laid out on its own).
void mark(brls::View* view, std::vector<brls::View*>& roots)
{
    if (dynamic_cast<brls::Label*>(view) && YGNodeHasMeasureFunc(view->getYGNode())) YGNodeMarkDirty(view->getYGNode());
    if (auto* digits = dynamic_cast<CadranDigits*>(view)) digits->remeasure();
    if (view->isDetached()) roots.push_back(view);
    if (auto* box = dynamic_cast<brls::Box*>(view))
        for (brls::View* child : box->getChildren()) mark(child, roots);
}

#ifdef __SWITCH__
AppletHookCookie s_cookie;

// On the dock or off it: next frame (libnx calls this inside the main loop).
void on_applet_hook(AppletHookType hook, void*)
{
    if (hook == AppletHookType_OnOperationMode) brls::sync([]() { apply(); });
}
#endif
}   // namespace

void apply()
{
    const float scale = larger() ? LARGE : 1.0f;
    if (scale == brls::Label::getFontScale()) return;
    brls::Label::setFontScale(scale);
    for (brls::Activity* activity : brls::Application::getActivitiesStack()) {
        brls::View* root = activity->getContentView();
        if (!root) continue;
        std::vector<brls::View*> roots;
        mark(root, roots);
        root->invalidate();
        for (brls::View* detached : roots) detached->invalidate();
    }
    // The tab on screen, for what it lays out by the size (Today's room).
    ui::refresh_shown_tab();
}

void install()
{
    apply();
#ifdef __SWITCH__
    appletHook(&s_cookie, on_applet_hook, nullptr);
#endif
}
}   // namespace text_size
