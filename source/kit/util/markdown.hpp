// markdown — GitHub-flavoured Markdown (a pull request's description, a
// release's notes) as blocks the console can draw (view/markdown_view.hpp):
// headings (ATX and setext), paragraphs (a line break inside one is kept, as
// GitHub does in descriptions), bullet, numbered and task lists (nested),
// fenced code, quotes and their alerts ("> [!NOTE]"), rules, tables, and
// the HTML GitHub allows reduced to its text (<details> keeps its
// <summary> as a heading, <br> breaks the line, comments go).
// Inline: links keep their text, images their alt text, emphasis and code
// marks go, HTML entities and backslash escapes are read, and the emoji
// (the console's font has none) go, but the one a heading or an item opens
// on, kept as `icon` (its code point in hex).
// Nothing in it is ever run or opened: it is text. It is cleaned first
// (util/safe_text.hpp), and bounded: at most MAX_BLOCKS blocks, MAX_CODE_LINES
// lines of a code block, MAX_COLUMNS columns and MAX_ROWS rows of a table,
// MAX_DEPTH levels of nesting. Plain C++ for the host tests (tests/markdown).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace markdown
{

constexpr size_t MAX_BLOCKS = 300;
constexpr size_t MAX_CODE_LINES = 60;
constexpr size_t MAX_COLUMNS = 6;
constexpr size_t MAX_ROWS = 40;
constexpr int    MAX_DEPTH = 4;

struct Block
{
    enum Kind
    {
        Heading,     // `level` 1 to 6
        Paragraph,   // `text`, its line breaks kept
        Item,        // a list item: `level` its depth (0: top), `number` (0: a bullet), `task`
        Code,        // `lines`, as written
        Quote,       // `text`; `alert`: "note", "tip", "important", "warning", "caution" or ""
        Rule,
        Table,       // `rows`, the first one the header, each cut to MAX_COLUMNS cells
    };
    Kind        kind = Paragraph;
    std::string text;
    int         level = 0;
    int         number = 0;
    int         task = -1;   // Item: -1 not a task, 0 open, 1 done
    std::string alert;
    std::string icon;        // Heading / Item: the emoji it opens on, in hex ("1f680")
    std::vector<std::string> lines;
    std::vector<std::vector<std::string>> rows;
};

std::vector<Block> parse(const std::string& md);

// One line's inline Markdown as plain text (links, images, emphasis, code,
// entities, escapes, HTML tags, emoji).
std::string inline_text(const std::string& md);

}   // namespace markdown
