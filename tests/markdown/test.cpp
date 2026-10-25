// Host tests for source/util/safe_text.cpp and markdown.cpp: what text from
// GitHub keeps (valid UTF-8, no control, bidi or zero-width character, cut
// between two characters; notes cut after a whole line), versions, and
// GitHub-flavoured Markdown as blocks
// (headings, paragraphs and their line breaks, lists, tasks, code, quotes and
// alerts, rules, tables, HTML, inline marks, entities, emoji) with its bounds.
#include "check.h"
#include <string>

#include "util/markdown.hpp"
#include "util/safe_text.hpp"

using markdown::Block;

static void test_safe_text()
{
    using safe_text::clean;
    CHECK(clean("plain", 100, false) == "plain");
    CHECK(clean("caf\xC3\xA9 \xE2\x9C\x93", 100, false) == "caf\xC3\xA9 \xE2\x9C\x93");   // café ✓ kept
    // Invalid UTF-8: a stray continuation byte, an overlong '/', a surrogate, past U+10FFFF, cut short.
    CHECK(clean("a\x80" "b", 100, false) == "ab");
    CHECK(clean("a\xC0\xAF" "b", 100, false) == "ab");
    CHECK(clean("a\xED\xA0\x80" "b", 100, false) == "ab");
    CHECK(clean("a\xF4\x90\x80\x80" "b", 100, false) == "ab");
    CHECK(clean("a\xE2\x82", 100, false) == "a");
    // Controls: gone; tab: a space; CRLF: LF; LF: a space unless multiline.
    CHECK(clean(std::string("a\0b\x1B[31mc\x7F", 9), 100, false) == "ab[31mc");
    CHECK(clean("a\xC2\x85" "b", 100, false) == "ab");   // NEL (C1)
    CHECK(clean("a\tb\r\nc", 100, true) == "a b\nc");
    CHECK(clean("a\nb", 100, false) == "a b");
    CHECK(clean("a\xE2\x80\xA8" "b", 100, true) == "a\nb");
    // Bidi overrides and isolates, marks, zero-width characters, BOM: gone
    // (a right-to-left override no longer makes "evil" + "txt.exe" read as "evilexe.txt").
    CHECK(clean("evil\xE2\x80\xAEtxt.exe", 100, false) == "eviltxt.exe");
    CHECK(clean("\xE2\x81\xA6x\xE2\x81\xA9\xE2\x80\x8F\xD8\x9C\xE2\x80\x8B\xE2\x80\x8C\xE2\x81\xA0\xEF\xBB\xBFy", 100, false) == "xy");
    // Cut between two characters, never inside one.
    CHECK(clean("ab\xC3\xA9", 3, false) == "ab");
    CHECK(clean("ab\xC3\xA9", 4, false) == "ab\xC3\xA9");

    // Notes: cleaned on several lines, cut at the end of a whole line.
    using safe_text::clean_notes;
    CHECK(clean_notes("one\r\ntwo\nthree", 100) == "one\ntwo\nthree");
    CHECK(clean_notes("one\ntwo\nthree", 13) == "one\ntwo\nthree" && clean_notes("one\ntwo\nthree", 12) == "one\ntwo");
    CHECK(clean_notes("one\ntwo\nthree", 7) == "one\ntwo" && clean_notes("one\ntwo\nthree", 6) == "one");
    CHECK(clean_notes("a long first line\nx", 5).empty() && clean_notes("", 0).empty());
    CHECK(clean_notes("a\x80\nb", 3) == "a\nb");   // measured once cleaned

    CHECK(safe_text::is_version("1.2.0") && safe_text::is_version("1.3.0-rc.1") && safe_text::is_version("2.0.0+build_5"));
    CHECK(!safe_text::is_version("") && !safe_text::is_version("../1.0") && !safe_text::is_version("1.0 beta") &&
          !safe_text::is_version("-1") && !safe_text::is_version("1.0\xE2\x80\xAE") && !safe_text::is_version(std::string(41, '1')));
}

static void test_inline()
{
    using markdown::inline_text;
    CHECK(inline_text("**bold** and *it* and _it_ and ~~gone~~") == "bold and it and it and gone");
    // "_" inside a word is no emphasis, "*" is (as on GitHub); alone, both stay.
    CHECK(inline_text("snake_case_name, 2*3*4, a * b _ c") == "snake_case_name, 234, a * b _ c");
    CHECK(inline_text("see [the docs](https://x/y_(z)) or [ref][1]") == "see the docs or ref");
    CHECK(inline_text("![logo](https://x/l.png) shot") == "logo shot");
    CHECK(inline_text("run `make **check**` now") == "run make **check** now");
    CHECK(inline_text("a <b>bold</b> <img src=x onerror=alert(1)> b") == "a bold b");
    CHECK(inline_text("line<br>next") == "line\nnext");
    CHECK(inline_text("<https://github.com>") == "https://github.com");
    CHECK(inline_text("a &amp; b &lt;c&gt; &#233; &#x2192; &bogus; &") == "a & b <c> \xC3\xA9 \xE2\x86\x92 &bogus; &");
    // A reference cannot bring a control or bidi character back.
    CHECK(inline_text("x&#x202E;y&#27;z&#0;") == "xyz&#0;");
    CHECK(inline_text("\\*not emphasis\\* \\[x\\]") == "*not emphasis* [x]");
    CHECK(inline_text("\xF0\x9F\x9A\x80 Features \xE2\x9C\xA8") == "Features");   // 🚀 … ✨
    CHECK(inline_text("[[[[[deep]]]]](u)") .find("deep") != std::string::npos);
}

static void test_blocks()
{
    const std::string md =
        "# Title\n"
        "Setext\n"
        "======\n"
        "## \xF0\x9F\x9A\x80 Features ##\n"
        "First line\n"
        "second line\n"
        "\n"
        "- item **one**\n"
        "  continued\n"
        "  - nested\n"
        "        - deep\n"
        "1. first\n"
        "12) twelfth\n"
        "- [x] done\n"
        "- [ ] todo\n"
        "* plain\n"
        "\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "  indented\n"
        "```\n"
        "> quoted\n"
        "> more\n"
        "\n"
        "> [!WARNING]\n"
        "> Careful\n"
        "\n"
        "---\n"
        "| a | b \\| c |\n"
        "|---|:-:|\n"
        "| 1 | 2 |\n"
        "| 3 |\n"
        "\n"
        "<!-- hidden\n"
        "still hidden -->\n"
        "<details>\n"
        "<summary>More</summary>\n"
        "inside <!-- gone --> text\n"
        "</details>\n";
    const std::vector<Block> b = markdown::parse(md);
    if (!CHECK(b.size() == 19)) {
        for (const Block& x : b) std::printf("  %d [%s]\n", (int)x.kind, x.text.c_str());
        return;
    }
    CHECK(b[0].kind == Block::Heading && b[0].level == 1 && b[0].text == "Title");
    CHECK(b[1].kind == Block::Heading && b[1].level == 1 && b[1].text == "Setext");
    CHECK(b[2].kind == Block::Heading && b[2].level == 2 && b[2].text == "Features" && b[2].icon == "1f680");
    CHECK(b[3].kind == Block::Paragraph && b[3].text == "First line\nsecond line");
    CHECK(b[4].kind == Block::Item && b[4].level == 0 && b[4].number == 0 && b[4].text == "item one continued" && b[4].task < 0);
    CHECK(b[5].kind == Block::Item && b[5].level == 1 && b[5].text == "nested");
    CHECK(b[6].kind == Block::Item && b[6].level == markdown::MAX_DEPTH && b[6].text == "deep");
    CHECK(b[7].kind == Block::Item && b[7].number == 1 && b[7].text == "first");
    CHECK(b[8].kind == Block::Item && b[8].number == 12 && b[8].text == "twelfth");
    CHECK(b[9].task == 1 && b[9].text == "done" && b[10].task == 0 && b[10].text == "todo");
    CHECK(b[11].kind == Block::Item && b[11].text == "plain");
    CHECK(b[12].kind == Block::Code && b[12].lines.size() == 2 && b[12].lines[1] == "  indented");
    CHECK(b[13].kind == Block::Quote && b[13].alert.empty() && b[13].text == "quoted\nmore");
    CHECK(b[14].kind == Block::Quote && b[14].alert == "warning" && b[14].text == "Careful");
    CHECK(b[15].kind == Block::Rule);
    CHECK(b[16].kind == Block::Table && b[16].rows.size() == 3 && b[16].rows[0].size() == 2 && b[16].rows[0][1] == "b | c" &&
          b[16].rows[1][0] == "1" && b[16].rows[2].size() == 1);
    CHECK(b[17].kind == Block::Heading && b[17].level == 4 && b[17].text == "More");
    CHECK(b[18].kind == Block::Paragraph && b[18].text == "inside text");
}

static void test_bounds()
{
    // Never more than MAX_BLOCKS blocks, MAX_CODE_LINES (and "…") code lines, MAX_COLUMNS columns.
    std::string many;
    for (int i = 0; i < 1000; i++) many += "- item\n";
    CHECK(markdown::parse(many).size() == markdown::MAX_BLOCKS);
    std::string code = "```\n";
    for (int i = 0; i < 500; i++) code += "line\n";
    const auto c = markdown::parse(code);   // never closed: to the end
    CHECK(c.size() == 1 && c[0].lines.size() == markdown::MAX_CODE_LINES + 1);
    const auto t = markdown::parse("|1|2|3|4|5|6|7|8|\n|-|-|-|-|-|-|-|-|\n");
    CHECK(t.size() == 1 && t[0].rows[0].size() == markdown::MAX_COLUMNS);
    // Hostile input: no crash, nothing left that could be read as markup by the view.
    CHECK(markdown::parse("").empty() && markdown::parse("\n\n\n").empty());
    CHECK(markdown::parse(std::string(100000, '[')).size() == 1);
    CHECK(markdown::parse(std::string(100000, '`')).size() <= 1);
    // Runs that used to be scanned to the end once per character.
    CHECK(markdown::parse(std::string(100000, '<')).size() == 1);
    CHECK(markdown::parse(std::string(100000, '<') + ">").size() == 1);
    CHECK(markdown::parse(std::string(100000, '&')).size() == 1);
    std::string scripted;
    for (int i = 0; i < 10000; i++) scripted += "<scripted ";
    CHECK(markdown::parse(scripted).size() == 1);   // not an element: its text stays
    CHECK(markdown::inline_text("a << b <br> c &amp; d &toolongname; e") == "a << b\nc & d &toolongname; e");
    CHECK(markdown::parse("<!--").empty());
    // Raw HTML elements go with what they hold, whatever their case; a
    // look-alike tag does not swallow the text after it.
    const auto html = markdown::parse("a <SCRIPT type=x>alert(1)</script> b <style>p{}</style>c <scripted> d <script>never closed");
    CHECK(html.size() == 1 && html[0].text == "a b c d");
    const auto bidi = markdown::parse("# a\xE2\x80\xAE" "b");
    CHECK(bidi.size() == 1 && bidi[0].text == "ab");
}

int main()
{
    test_safe_text();
    test_inline();
    test_blocks();
    test_bounds();
    return CHECK_DONE("safe text, versions, inline and block Markdown, and bounds assertions passed");
}
