// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/whats_new_activity.hpp"

#include "activity/changelog_activity.hpp"
#include "app.hpp"
#include "view/funding.hpp"
#include "view/release_notes.hpp"

using namespace brls::literals;

void WhatsNewActivity::onContentAvailable()
{
    if (auto* applet = dynamic_cast<brls::AppletFrame*>(this->getContentView()))
        applet->setTitle("playshelf/title"_i18n + " " + app::version());
    std::string date;
    const auto lines      = release_notes::current(&date);
    const auto highlights = release_notes::highlights();
    header->setTitle(brls::getStr("playshelf/about/section_changelog", app::version()));
    if (!date.empty()) header->setSubtitle(date);
    // The highlights, the full list a press away; the full list when there are none.
    if (highlights.empty()) release_notes::fill(notes, lines);
    else release_notes::fill_highlights(notes, highlights);
    full->setVisibility(!highlights.empty() && !lines.empty() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    full->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new ChangelogActivity());
        return true;
    });
    // The notes take the first focus (with no highlight), so the screen opens
    // at its top rather than scrolled to Close.
    notes->setFocusable(true);
    notes->setHideHighlight(true);
    support_note->setSingleLine(false);
    funding_box->addView(funding::cards(180, true));
    close->registerClickAction([](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
}
