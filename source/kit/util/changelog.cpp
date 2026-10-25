// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/changelog.hpp"

#include <cctype>
#include <cstdio>
#include <sstream>

#include "util/emoji.hpp"
#include "util/text.hpp"

namespace changelog
{
namespace
{
// The emoji `s` opens on (after spaces and "**"), as its first code point in
// hex: "1f680" for "🚀 Features". Empty when it opens on anything else.
std::string leading_icon(const std::string& s)
{
    const size_t i = s.find_first_not_of(" *");
    return i == std::string::npos ? "" : emoji::icon_at(s, i);
}

// "abc1234" (a commit) or "#12" (a pull request or an issue).
bool is_reference(const std::string& s)
{
    if (s.size() > 1 && s[0] == '#') {
        for (size_t i = 1; i < s.size(); i++)
            if (!std::isdigit((unsigned char)s[i])) return false;
        return true;
    }
    if (s.size() < 7 || s.size() > 40) return false;
    for (char c : s)
        if (!std::isxdigit((unsigned char)c)) return false;
    return true;
}

// "[text](url)" → "text"; " ([#12](url))" and " ([abc1234](url))" go.
std::string strip_links(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '[') {
            const size_t close = s.find(']', i);
            if (close != std::string::npos && close + 1 < s.size() && s[close + 1] == '(') {
                const size_t end = s.find(')', close);
                if (end != std::string::npos) {
                    const std::string text = s.substr(i + 1, close - i - 1);
                    // A reference wrapped in parentheses: drop the whole group.
                    if (is_reference(text) && !out.empty() && out.back() == '(' && end + 1 < s.size() && s[end + 1] == ')') {
                        out.pop_back();
                        while (!out.empty() && out.back() == ' ') out.pop_back();
                        i = end + 2;
                    } else {
                        out += text;
                        i = end + 1;
                    }
                    continue;
                }
            }
        }
        out += s[i++];
    }
    return out;
}

std::string clean(const std::string& s)
{
    std::string out;
    for (char c : strip_links(emoji::drop(s)))
        if (c != '`') out += c;
    for (size_t p; (p = out.find("**")) != std::string::npos;) out.erase(p, 2);
    return text::trim_space(out);
}

// "1.1.0 (2026-10-10)" → "1.1.0 · 2026-10-10".
std::string release_title(std::string s)
{
    const size_t open = s.rfind(" (");
    if (open != std::string::npos && s.back() == ')')
        s = s.substr(0, open) + " · " + s.substr(open + 2, s.size() - open - 3);
    return s;
}

// "**Title**: text" or "**Title:** text" → "Title" and "text"; false when
// `item` does not open on a bold lead-in.
bool split_title(const std::string& item, std::string* title, std::string* text)
{
    const size_t open = item.find("**");   // after the emoji, if any
    if (open == std::string::npos || !text::trim_space(emoji::drop(item.substr(0, open))).empty()) return false;
    const size_t close = item.find("**", open + 2);
    if (close == std::string::npos) return false;
    std::string head = clean(item.substr(open + 2, close - open - 2));
    std::string rest = item.substr(close + 2);
    if (!head.empty() && head.back() == ':') head.pop_back();
    else if (text::starts_with(rest, ":")) rest.erase(0, 1);
    else return false;   // bold words inside a sentence
    *title = text::trim_space(head);
    *text  = clean(rest);
    return !title->empty();
}

}   // namespace

std::vector<Line> parse(const std::string& md, int max_releases)
{
    std::vector<Line> lines;
    std::istringstream in(md);
    std::string raw;
    int  releases = 0;
    bool in_item  = false;   // an indented line continues the last item
    bool stop     = false;   // past `max_releases`
    // One line of Markdown; `indented` keeps its indentation, `t` is trimmed.
    auto line = [&](const std::string& indented, const std::string& t) {
        if (t.empty()) {
            in_item = false;
            return;
        }
        if (t.front() == '<' && t.back() == '>') {
            in_item = false;   // an HTML line: the release page's <details>, its figures
            return;
        }
        if (text::starts_with(t, "## ")) {
            if (max_releases > 0 && ++releases > max_releases) {
                stop = true;
                return;
            }
            lines.push_back({ Line::Release, release_title(clean(t.substr(3))) });
            in_item = false;
        } else if (text::starts_with(t, "# ")) {
            in_item = false;   // the document's title
        } else if (text::starts_with(t, "### ")) {
            lines.push_back({ Line::Section, clean(t.substr(4)) });
            lines.back().icon = leading_icon(t.substr(4));
            in_item = false;
        } else if (text::starts_with(t, "* ") || text::starts_with(t, "- ")) {
            const size_t indent = indented.find_first_not_of(' ');
            Line item{ Line::Item, clean(t.substr(2)), (int)(indent / 2) };
            if (item.depth == 0) {
                split_title(t.substr(2), &item.title, &item.text);
                item.icon = leading_icon(t.substr(2));
            }
            lines.push_back(std::move(item));
            in_item = true;
        } else if (in_item && indented[0] == ' ') {
            // "* **Title**:" then its text on the next line (the release page's
            // two-line bullets): no leading space.
            std::string& text = lines.back().text;
            text += (text.empty() ? "" : " ") + clean(t);
        } else {
            lines.push_back({ Line::Text, clean(t) });
            in_item = false;
        }
        if (!lines.empty() && lines.back().text.empty() && lines.back().title.empty()) lines.pop_back();
    };

    // HTML comments, invisible on GitHub: skipped, except the console's own
    // (text::ConsoleComments). A comment or a block ends the item before it.
    text::ConsoleComments comments;
    while (!stop && std::getline(in, raw)) {
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        const std::string t = text::trim_space(raw);
        std::string inner;
        const auto step = comments.next(t, inner);
        if (step == text::ConsoleComments::Skip) continue;
        if (step != text::ConsoleComments::Show) {
            in_item = false;
            if (step == text::ConsoleComments::Directive) {
                line(inner, inner);
                in_item = false;
            }
            continue;
        }
        line(raw, t);
    }
    return lines;
}

std::vector<Line> release_notes(const std::string& md, const std::string& version, std::string* date)
{
    auto bare = [](const std::string& v) { return !v.empty() && (v[0] == 'v' || v[0] == 'V') ? v.substr(1) : v; };
    const std::string wanted = bare(version);
    std::vector<Line> notes;
    bool inside = false;
    for (Line& l : parse(md, 0)) {
        if (l.kind == Line::Release) {
            if (inside) break;
            const size_t dot = l.text.find(" · ");
            if (wanted.empty() || bare(l.text.substr(0, dot)) != wanted) continue;
            inside = true;
            if (date) *date = dot == std::string::npos ? "" : l.text.substr(dot + std::string(" · ").size());
        } else if (inside) {
            notes.push_back(std::move(l));
        }
    }
    return notes;
}

size_t summary_end(const std::vector<Line>& lines)
{
    // release-please-config.json's sections, their emoji dropped, and the
    // breaking changes it lists first.
    static const char* const LISTS[] = { "BREAKING CHANGES", "Features", "Changes", "Bug Fixes", "Polish", "Other",
                                         "Tests", "Documentation" };
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].kind != Line::Section) continue;
        for (const char* name : LISTS)
            if (lines[i].text == name) return i;
    }
    return lines.size();
}

}   // namespace changelog
