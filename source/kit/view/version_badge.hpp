// VersionBadge — the running version, next to every screen's title.
// A release: its version in a grey tag ("1.2.0"). Any other build (main, a
// pull request, a local one): the version in an amber tag, then the commit
// ("1.2.0 1a2b3c4"), so a development build is never taken for the published
// one, on the console or in a screenshot.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class VersionBadge : public brls::Box
{
  public:
    VersionBadge();

    // Puts a badge by the title of every screen, when it first takes the
    // focus. Called once, at start-up, before the first screen opens.
    static void install();

    // The row at the right of `frame`'s header (the bell, the language and the
    // theme go in it), made the first time it is asked for; null for a frame
    // without the applet frame's header.
    static brls::Box* header_tools(brls::AppletFrame* frame);
};
