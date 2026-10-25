// changelog — CHANGELOG.md (bundled in the romfs) as lines the console can show: the
// release headings, their sections and items, as plain text. Plain C++ so the
// host tests can run it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace changelog
{

struct Line
{
    enum Kind
    {
        Release,   // "## [1.1.0](…) (2026-10-10)" → "1.1.0 · 2026-10-10"
        Section,   // "### 🚀 Features" → "Features"
        Item,      // "* **ui:** text ([#12](…)) ([abc1234](…))" → "ui" and "text", indented by `depth`
        Text,      // any other paragraph line
    };
    Kind        kind;
    std::string text;
    int         depth = 0;   // Item only: 0 for a top-level bullet
    std::string title = {};  // top-level Item only: its "**Title**:" lead-in
    std::string icon  = {};  // Section or top-level Item: the emoji it opens on, as its
                             // code point in hex ("1f680" for 🚀), for an image of it
};

// The lines of `md`, at most `max_releases` "## " headings and what follows
// them (0: all). Links keep their text, commit and pull-request references
// are dropped, and so are the emoji (the console's font has none: the one a
// section or an item opens on is its `icon`) and the "**" / "`" marks. A
// top-level item that opens on a bold lead-in ("* **Activity**: play time per
// game", "* **ui:** text") has it as its `title`, the rest as its text. The
// "# Changelog" title is left out, and so are HTML lines ("<details>",
// "<summary>…</summary>": a release's notes fold the commit list) and HTML
// comments, but for the console's own: "<!-- console: a line -->" is read as
// that line, the lines of a "<!-- console:only" … "-->" comment are read, and
// those between "<!-- console:hide -->" and "<!-- /console -->" are not (the
// release page's GitHub-only Markdown).
std::vector<Line> parse(const std::string& md, int max_releases);

// The sections and items under the "## " heading of `version` ("1.1.0", a
// leading "v" in either is ignored); empty when there is none (a development
// build). `date`, if given, receives the heading's date ("2026-10-10").
std::vector<Line> release_notes(const std::string& md, const std::string& version, std::string* date = nullptr);

// Where the summary written for a release (its "### 🌟 Highlights" and the
// like) ends in its notes: at the first of release-please's own sections
// ("Features", "Bug Fixes", … as in release-please-config.json), else at the
// end. 0: no summary.
size_t summary_end(const std::vector<Line>& lines);

}   // namespace changelog
