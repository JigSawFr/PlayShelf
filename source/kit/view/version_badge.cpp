// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/version_badge.hpp"

#include "app.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/ui.hpp"

#include <algorithm>

static constexpr const char* BADGE_ID = "version_badge";
static constexpr const char* TOOLS_ID = "header_tools";

VersionBadge::VersionBadge()
    : brls::Box(brls::Axis::ROW)
{
    this->setId(BADGE_ID);
    this->setFocusable(false);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setAlignSelf(brls::AlignSelf::CENTER);
    this->setMarginLeft(16);
    // Level with the title, which borealis moves down by this much, less 8:
    // centred like the title's box, the tag and the commit sat 4 px below
    // the middle of its capitals (the larger font has more room under them),
    // and a centred box moves by half its margin.
    this->setMarginTop(brls::getStyle()["brls/applet_frame/header_title_top_offset"] - 8);

    // On the desktop, one made-up build (PLAYSHELF_SIM_VERSION, see app.cpp).
    const std::string version = app::version(), commit = app::commit();
    const bool release = app::is_release();

    // The tag: filled (the amber of the warnings, or the notes' grey for a
    // release), the version in the background's colour.
    const NVGcolor ink = release ? cadran::kit::color("cadran/text3") : cadran::kit::color("cadran/amber");
    auto* tag = new brls::Box(brls::Axis::ROW);
    tag->setBackgroundColor(ink);
    tag->setCornerRadius(4);
    tag->setPadding(2, 8, 2, 8);
    auto* number = new brls::Label();
    number->setText(version);
    number->setFontSize(18);
    number->setFontScaled(false);   // a tag: the same size with the larger text
    number->setSingleLine(true);
    number->setTextColor(brls::Application::getTheme()["brls/background"]);
    tag->addView(number);
    this->addView(tag);
    if (release || commit.empty()) return;
    auto* text = new brls::Label();
    text->setFontSize(18);
    text->setFontScaled(false);
    text->setSingleLine(true);
    text->setMarginLeft(10);
    text->setText(commit);
    text->setTextColor(ink);
    this->addView(text);
}

// Adds the badge right after the title of the screen holding `focus` (its
// root view: borealis gives a new screen the focus before putting it on the
// activity stack), unless it has one. Screens without the applet frame's
// header (the loading screen, dialogs) are left alone.
static void badge_screen_of(brls::View* focus)
{
    if (!focus) return;
    while (focus->hasParent()) focus = focus->getParent();
    auto* frame = dynamic_cast<brls::AppletFrame*>(focus);
    // Not on the child screen: a version means nothing to a child, and its
    // amber reads as a warning.
    if (frame && frame->getView("child_root")) return;
    auto* label = frame ? frame->getView("brls/applet_frame/title_label") : nullptr;
    auto* row   = label ? dynamic_cast<brls::Box*>(label->getParent()) : nullptr;
    if (!row || row->getView(BADGE_ID)) return;
    const auto& views = row->getChildren();
    const auto at = std::find(views.begin(), views.end(), label) - views.begin() + 1;
    row->addView(new VersionBadge(), at);
}

brls::Box* VersionBadge::header_tools(brls::AppletFrame* frame)
{
    auto* header = frame ? dynamic_cast<brls::Box*>(frame->getView("brls/applet_frame/header")) : nullptr;
    if (!header) return nullptr;
    if (auto* tools = dynamic_cast<brls::Box*>(header->getView(TOOLS_ID))) return tools;
    auto* tools = new brls::Box(brls::Axis::ROW);
    tools->setId(TOOLS_ID);
    tools->setFocusable(false);
    tools->setAlignItems(brls::AlignItems::CENTER);
    tools->setAlignSelf(brls::AlignSelf::CENTER);
    header->addView(tools);
    return tools;
}

void VersionBadge::install()
{
    brls::Application::getGlobalFocusChangeEvent()->subscribe(badge_screen_of);
}
