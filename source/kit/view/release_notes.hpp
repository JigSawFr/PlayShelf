// release_notes — PlayShelf's release notes, as views: on the "What's new"
// screen shown once after an update, the hand-written highlights when there
// are some, the full list (the bundled CHANGELOG.md) a press away; on the
// release notes screen (About › Release notes), a release's both together.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <string>
#include <vector>

#include "util/changelog.hpp"

namespace release_notes
{

// This version's lines (empty: none, e.g. a development build); `date`
// receives the release date when there is one.
std::vector<changelog::Line> current(std::string* date = nullptr);

// This version's highlights: the summary written by hand for its release
// (.github/release-notes/<version>.md, bundled as release-notes/<version>.md).
// Empty when there is none.
std::vector<changelog::Line> highlights();

// Both, as a release's notes read: the highlights, then this version's lines
// (fill_release() tells them apart).
std::vector<changelog::Line> bundled();

// Adds `lines` to `box`: section titles (their emoji as an icon, over a
// rule), then their items with the bullet (or their emoji) in its own column,
// wrapped lines staying under the text, and their lead-in ("activity") as a tag.
void fill(aurore::Box* box, const std::vector<changelog::Line>& lines);

// Adds the highlights to `box`: their opening line, the items as cards two
// by two (its emoji as an icon, its "**Title**:" in the accent colour over
// its text; with no title, the item itself in the accent colour), then the
// closing lines as a note.
void fill_highlights(aurore::Box* box, const std::vector<changelog::Line>& lines);

// Adds a release's notes to `box`: its summary (changelog::summary_end()) as
// highlights, then release-please's lists as by fill().
void fill_release(aurore::Box* box, const std::vector<changelog::Line>& lines);

}   // namespace release_notes
