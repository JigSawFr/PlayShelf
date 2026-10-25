// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/changelog_activity.hpp"

#include "app.hpp"
#include "view/release_notes.hpp"

using namespace brls::literals;

void ChangelogActivity::onContentAvailable()
{
    if (auto* applet = dynamic_cast<brls::AppletFrame*>(this->getContentView()))
        applet->setTitle("playshelf/title"_i18n + " " + app::version());
    std::string date;
    const auto lines = release_notes::current(&date);
    header->setTitle(brls::getStr("playshelf/about/section_changelog", app::version()));
    if (!date.empty()) header->setSubtitle(date);
    release_notes::fill(notes, lines);
    // Nothing else to focus: the notes take it (with no highlight), so the
    // stick and the D-pad scroll them.
    notes->setFocusable(true);
    notes->setHideHighlight(true);
    note->setSingleLine(false);
    note->setText(brls::getStr("playshelf/about/changelog_note", std::string(app::repo_url()) + "/releases"));
}
