// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/markdown.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "util/emoji.hpp"
#include "util/safe_text.hpp"
#include "util/text.hpp"
#include "util/utf8.hpp"

namespace markdown
{

namespace
{
size_t indent_of(const std::string& line)
{
    size_t n = 0;
    for (char c : line) {
        if (c == ' ') n++;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

// "&amp;" and the like at s[i]: its text and length; 0 when it is not one.
size_t entity(const std::string& s, size_t i, std::string* out)
{
    static const struct { const char* name; const char* text; } NAMED[] = {
        { "amp", "&" }, { "lt", "<" }, { "gt", ">" }, { "quot", "\"" }, { "apos", "'" }, { "nbsp", " " },
        { "mdash", "\xE2\x80\x94" }, { "ndash", "\xE2\x80\x93" }, { "hellip", "\xE2\x80\xA6" },
        { "rarr", "\xE2\x86\x92" }, { "larr", "\xE2\x86\x90" }, { "middot", "\xC2\xB7" }, { "copy", "\xC2\xA9" },
    };
    // Only the next 11 characters: a longer name is no entity, and looking
    // further would scan the rest of the text again for every '&'.
    const size_t at = s.substr(i, 11).find(';');
    if (at == std::string::npos || at == 1) return 0;
    const size_t semi = i + at;
    const std::string name = s.substr(i + 1, semi - i - 1);
    if (name[0] == '#') {
        uint32_t cp = 0;
        const bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
        const std::string digits = name.substr(hex ? 2 : 1);
        if (digits.empty() || digits.size() > 6) return 0;
        for (char c : digits) {
            if (hex ? !std::isxdigit((unsigned char)c) : !std::isdigit((unsigned char)c)) return 0;
            cp = cp * (hex ? 16 : 10) + (uint32_t)(std::isdigit((unsigned char)c) ? c - '0' : std::tolower(c) - 'a' + 10);
        }
        if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
        std::string text;
        utf8::append(text, cp);
        // What a reference spells out is cleaned like the rest (no control
        // or bidi character sneaks in that way).
        *out = safe_text::clean(text, 8, false);
        return semi - i + 1;
    }
    for (const auto& e : NAMED)
        if (name == e.name) {
            *out = e.text;
            return semi - i + 1;
        }
    return 0;
}

// The end of the bracketed text opening at s[i] ('[' or '('), nesting
// counted; npos when it does not close within 1000 characters (CommonMark's
// own bound on a link's text), so a run of '[' is not scanned to the end
// once per '['.
size_t closing(const std::string& s, size_t i, char open, char close)
{
    int depth = 0;
    const size_t last = std::min(s.size(), i + 1000);
    for (size_t k = i; k < last; k++) {
        if (s[k] == '\\') {
            k++;
            continue;
        }
        if (s[k] == open) depth++;
        else if (s[k] == close && --depth == 0) return k;
    }
    return std::string::npos;
}

bool word(char c)
{
    return std::isalnum((unsigned char)c) || (unsigned char)c >= 0x80;
}

std::string inline_pass(const std::string& s, int depth)
{
    std::string out;
    size_t next_gt = s.find('>');
    for (size_t i = 0; i < s.size();) {
        const char c = s[i];
        // \* : the character itself.
        if (c == '\\' && i + 1 < s.size() && std::ispunct((unsigned char)s[i + 1])) {
            out += s[i + 1];
            i += 2;
            continue;
        }
        // `code`: its text as it is.
        if (c == '`') {
            size_t ticks = 0;
            while (i + ticks < s.size() && s[i + ticks] == '`') ticks++;
            const size_t end = s.find(std::string(ticks, '`'), i + ticks);
            if (end != std::string::npos) {
                out += text::trim_blank(s.substr(i + ticks, end - i - ticks));
                i = end + ticks;
                continue;
            }
            out.append(ticks, '`');
            i += ticks;
            continue;
        }
        // ![alt](url) and [text](url) / [text][ref]: the text.
        const bool image = c == '!' && i + 1 < s.size() && s[i + 1] == '[';
        if ((c == '[' || image) && depth < 3) {
            const size_t open = image ? i + 1 : i;
            const size_t close = closing(s, open, '[', ']');
            if (close != std::string::npos && close + 1 < s.size() && (s[close + 1] == '(' || s[close + 1] == '[')) {
                const size_t end = s[close + 1] == '(' ? closing(s, close + 1, '(', ')') : s.find(']', close + 1);
                if (end != std::string::npos) {
                    out += inline_pass(s.substr(open + 1, close - open - 1), depth + 1);
                    i = end + 1;
                    continue;
                }
            }
        }
        // <https://…>: the address; <tag …> and </tag>: gone; <br>: a line break.
        if (c == '<') {
            // The next '>' is looked for once, not again for every '<' before it.
            if (next_gt != std::string::npos && next_gt < i) next_gt = s.find('>', i);
            const size_t end = next_gt;
            // An address or a tag starts with a letter, '/' or '!', and is
            // then taken whole: what lies up to the '>' is copied only then,
            // never once per '<' of a run like "<<<<…>".
            const unsigned char first = end != std::string::npos && end > i + 1 ? (unsigned char)s[i + 1] : 0;
            if (std::isalpha(first) || first == '/' || first == '!') {
                const std::string inside = s.substr(i + 1, end - i - 1);
                if (text::starts_with(inside, "http://") || text::starts_with(inside, "https://")) out += inside;
                else if (text::starts_with(text::lower_ascii(inside), "br")) out += '\n';
                i = end + 1;
                continue;
            }
        }
        if (c == '&') {
            std::string text;
            if (const size_t n = entity(s, i, &text)) {
                out += text;
                i += n;
                continue;
            }
        }
        // ** __ ~~ and a single * or _ at the edge of a word: emphasis marks.
        if ((c == '*' || c == '_' || c == '~') && i + 1 < s.size() && s[i + 1] == c) {
            i += 2;
            continue;
        }
        if (c == '*' || c == '_') {
            const bool before = i > 0 && word(s[i - 1]);
            const bool after = i + 1 < s.size() && word(s[i + 1]);
            if (before != after || (c == '*' && before && after)) {
                i++;
                continue;
            }
        }
        out += c;
        i++;
    }
    return out;
}

// "  " runs of spaces become one (what HTML does when GitHub shows them).
std::string squeeze(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == ' ' && !out.empty() && (out.back() == ' ' || out.back() == '\n')) continue;
        if (c == '\n') while (!out.empty() && out.back() == ' ') out.pop_back();
        out += c;
    }
    return text::trim_blank(out);
}

bool is_rule(const std::string& t)
{
    if (t.size() < 3) return false;
    const char c = t[0];
    if (c != '-' && c != '*' && c != '_') return false;
    size_t marks = 0;
    for (char k : t) {
        if (k == c) marks++;
        else if (k != ' ') return false;
    }
    return marks >= 3;
}

// "| a | b |" → "a", "b" (an escaped "\|" stays inside its cell).
std::vector<std::string> cells(std::string t)
{
    if (!t.empty() && t.front() == '|') t.erase(0, 1);
    if (!t.empty() && t.back() == '|' && (t.size() < 2 || t[t.size() - 2] != '\\')) t.pop_back();
    std::vector<std::string> out;
    std::string cell;
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == '\\' && i + 1 < t.size() && t[i + 1] == '|') {
            cell += '|';
            i++;
        } else if (t[i] == '|') {
            out.push_back(text::trim_blank(cell));
            cell.clear();
        } else {
            cell += t[i];
        }
    }
    out.push_back(text::trim_blank(cell));
    return out;
}

bool is_table_rule(const std::string& t)
{
    if (t.find('-') == std::string::npos || t.find('|') == std::string::npos) return false;
    for (const std::string& c : cells(t)) {
        if (c.empty()) return false;
        for (char k : c)
            if (k != '-' && k != ':' && k != ' ') return false;
    }
    return true;
}

// A list marker at the start of `t`: "- ", "* ", "+ ", "12. " or "12) ";
// its length (0: none) and its number (0: a bullet).
size_t list_marker(const std::string& t, int* number)
{
    if (t.size() >= 2 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ') {
        *number = 0;
        return 2;
    }
    size_t k = 0;
    while (k < t.size() && k < 9 && std::isdigit((unsigned char)t[k])) k++;
    if (k == 0 || k + 1 >= t.size() || (t[k] != '.' && t[k] != ')') || t[k + 1] != ' ') return 0;
    *number = std::atoi(t.substr(0, k).c_str());
    if (*number <= 0) *number = 1;
    return k + 2;
}

struct Parser
{
    std::vector<Block> blocks;
    bool open_paragraph = false;   // the last block is a paragraph still taking lines
    bool open_item = false;        // the last block is an item taking continuation lines
    bool open_quote = false;
    bool in_comment = false;       // inside a <!-- … --> that spans lines
    bool full = false;
    std::vector<std::string> lines;   // the text's, as parse() split it

    void add(Block b)
    {
        open_paragraph = open_item = open_quote = false;
        if (blocks.size() >= MAX_BLOCKS) {
            full = true;
            return;
        }
        blocks.push_back(std::move(b));
    }

    void text_block(Block::Kind kind, const std::string& raw, int level = 0)
    {
        Block b;
        b.kind = kind;
        b.level = level;
        std::string t = text::trim_blank(raw);
        b.icon = kind == Block::Heading || kind == Block::Item ? emoji::icon_at(squeeze(inline_pass(t, 0))) : "";
        b.text = inline_text(t);
        if (b.text.empty() && kind != Block::Item) return;
        add(std::move(b));
    }

    // The steps of parse(), one kind of block each, in the order they are
    // tried: a step given line n (`t`: trimmed, comments out) says whether
    // it took the line, and moves n to the last line it took.

    // <!-- … --> on one line or several: out of `t`. False when the whole
    // line is inside a comment (the line is skipped, nothing closed).
    bool uncomment(std::string& t)
    {
        if (in_comment) {
            const size_t end = t.find("-->");
            if (end == std::string::npos) return false;
            in_comment = false;
            t = text::trim_blank(t.substr(end + 3));
        }
        for (size_t open; (open = t.find("<!--")) != std::string::npos;) {
            const size_t end = t.find("-->", open + 4);
            if (end == std::string::npos) {
                in_comment = true;
                t = text::trim_blank(t.substr(0, open));
                break;
            }
            t = text::trim_blank(t.substr(0, open) + t.substr(end + 3));
        }
        return true;
    }

    // ``` or ~~~ fences: the lines as they are, to the closing fence.
    bool fence(size_t& n, const std::string& t)
    {
        if (!text::starts_with(t, "```") && !text::starts_with(t, "~~~")) return false;
        const std::string mark = t.substr(0, 3);
        Block b;
        b.kind = Block::Code;
        size_t k = n + 1;
        for (; k < lines.size() && !text::starts_with(text::trim_blank(lines[k]), mark.c_str()); k++) {
            if (b.lines.size() < MAX_CODE_LINES) b.lines.push_back(lines[k]);
            else if (b.lines.size() == MAX_CODE_LINES) b.lines.push_back("\xE2\x80\xA6");   // …
        }
        n = k;
        if (!b.lines.empty()) add(std::move(b));
        return true;
    }

    // <details> and the like: <summary> as a heading, the rest as text.
    bool details(const std::string& t)
    {
        if (t[0] != '<') return false;
        const std::string tag = text::lower_ascii(t);
        if (text::starts_with(tag, "<summary")) {
            text_block(Block::Heading, t, 4);
            return true;
        }
        if (inline_text(t).empty()) {   // a line of tags only
            open_paragraph = open_item = open_quote = false;
            return true;
        }
        return false;
    }

    // Setext headings: a paragraph's single line underlined.
    bool setext_heading(const std::string& t)
    {
        if (!open_paragraph || blocks.empty() || blocks.back().text.find('\n') != std::string::npos ||
            (t.find_first_not_of('=') != std::string::npos && t.find_first_not_of('-') != std::string::npos))
            return false;
        blocks.back().kind = Block::Heading;
        blocks.back().level = t[0] == '=' ? 1 : 2;
        open_paragraph = false;
        return true;
    }

    bool rule(const std::string& t)
    {
        if (!is_rule(t)) return false;
        Block b;
        b.kind = Block::Rule;
        add(std::move(b));
        return true;
    }

    bool heading(const std::string& t)
    {
        if (t[0] != '#') return false;
        size_t level = 0;
        while (level < t.size() && t[level] == '#') level++;
        if (level > 6 || (level != t.size() && t[level] != ' ')) return false;
        std::string text = text::trim_blank(t.substr(level));
        while (!text.empty() && text.back() == '#') text.pop_back();   // "## Title ##"
        text_block(Block::Heading, text, (int)level);
        return true;
    }

    // > quotes, and GitHub's alerts ("> [!WARNING]").
    bool quote(const std::string& t)
    {
        if (t[0] != '>') return false;
        std::string inside = text::trim_blank(t.substr(1));
        while (!inside.empty() && inside[0] == '>') inside = text::trim_blank(inside.substr(1));   // nested: flattened
        const std::string tag = text::lower_ascii(inside);
        for (const char* alert : { "note", "tip", "important", "warning", "caution" })
            if (tag == std::string("[!") + alert + "]") {
                Block b;
                b.kind = Block::Quote;
                b.alert = alert;
                add(std::move(b));
                open_quote = true;
                inside.clear();
            }
        if (inside.empty()) return true;
        const std::string text = inline_text(inside);
        if (open_quote && !blocks.empty()) {
            std::string& q = blocks.back().text;
            q += (q.empty() ? "" : "\n") + text;
        } else {
            Block b;
            b.kind = Block::Quote;
            b.text = text;
            add(std::move(b));
            open_quote = true;
        }
        return true;
    }

    // A table: a row of cells, then a row of dashes.
    bool table(size_t& n, const std::string& t)
    {
        if (t.find('|') == std::string::npos || n + 1 >= lines.size() || !is_table_rule(text::trim_blank(lines[n + 1])))
            return false;
        Block b;
        b.kind = Block::Table;
        auto row = [&](const std::string& line) {
            std::vector<std::string> r;
            for (const std::string& c : cells(text::trim_blank(line))) {
                if (r.size() >= MAX_COLUMNS) break;
                r.push_back(inline_text(c));
            }
            return r;
        };
        b.rows.push_back(row(t));
        size_t k = n + 2;
        for (; k < lines.size() && text::trim_blank(lines[k]).find('|') != std::string::npos; k++)
            if (b.rows.size() < MAX_ROWS + 1) b.rows.push_back(row(lines[k]));
        n = k - 1;
        add(std::move(b));
        return true;
    }

    // List items: their depth from their indentation (two to four spaces a level).
    bool item(const std::string& raw, const std::string& t)
    {
        int number = 0;
        const size_t len = list_marker(t, &number);
        if (!len) return false;
        Block b;
        b.kind = Block::Item;
        b.number = number;
        const size_t indent = indent_of(raw);
        b.level = (int)(indent / 2);
        if (b.level > MAX_DEPTH) b.level = MAX_DEPTH;
        std::string rest = t.substr(len);
        if (text::starts_with(rest, "[ ] ") || rest == "[ ]") b.task = 0;
        else if (text::starts_with(text::lower_ascii(rest), "[x] ") || text::lower_ascii(rest) == "[x]") b.task = 1;
        if (b.task >= 0) rest = rest.size() > 3 ? rest.substr(4) : "";
        b.icon = emoji::icon_at(squeeze(inline_pass(rest, 0)));
        b.text = inline_text(rest);
        add(std::move(b));
        open_item = true;
        return true;
    }

    // An indented line after an item continues it; any other line
    // continues the paragraph, on a line of its own.
    void paragraph(const std::string& raw, const std::string& t)
    {
        const std::string text = inline_text(t);
        if (text.empty()) return;
        if (open_item && indent_of(raw) > 0 && !blocks.empty()) {
            std::string& last = blocks.back().text;
            last += (last.empty() ? "" : " ") + text;
            return;
        }
        if (open_paragraph && !blocks.empty()) {
            blocks.back().text += "\n" + text;
            return;
        }
        Block b;
        b.kind = Block::Paragraph;
        b.text = text;
        add(std::move(b));
        open_paragraph = true;
    }
};
}   // namespace

std::string inline_text(const std::string& md)
{
    return squeeze(emoji::drop(inline_pass(md, 0)));
}

namespace
{
// <script>…</script>, <style>…</style> and the like: their content goes
// too, as GitHub drops it (not just their tags, which would leave the code).
std::string drop_raw_elements(std::string md)
{
    std::string low = text::lower_ascii(md);
    for (const char* name : { "script", "style", "textarea", "title", "iframe", "noscript" }) {
        const std::string open = std::string("<") + name, close = std::string("</") + name;
        for (size_t at = 0; (at = low.find(open, at)) != std::string::npos;) {
            const char next = at + open.size() < low.size() ? low[at + open.size()] : '>';
            if (next != '>' && next != ' ' && next != '\n' && next != '/') {   // "<scripted>": not this element
                low[at] = ' ';
                continue;
            }
            size_t end = low.find(close, at);
            end = end == std::string::npos ? low.size() : low.find('>', end);
            end = end == std::string::npos ? low.size() : end + 1;
            md.erase(at, end - at);
            low.erase(at, end - at);
        }
    }
    return md;
}
}   // namespace

std::vector<Block> parse(const std::string& source)
{
    const std::string md = drop_raw_elements(safe_text::clean(source, source.size(), true));
    Parser p;
    // safe_text::clean() left no \r for lines() to drop.
    p.lines = text::lines(md);
    for (size_t n = 0; n < p.lines.size() && !p.full; n++) {
        const std::string& raw = p.lines[n];
        std::string t = text::trim_blank(raw);
        if (!p.uncomment(t)) continue;
        if (t.empty()) {
            p.open_paragraph = p.open_item = p.open_quote = false;
            continue;
        }
        if (p.fence(n, t) || p.details(t) || p.setext_heading(t) || p.rule(t) || p.heading(t) || p.quote(t) ||
            p.table(n, t) || p.item(raw, t))
            continue;
        p.paragraph(raw, t);
    }
    return p.blocks;
}

}   // namespace markdown
