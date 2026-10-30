// Host tests for source/util/changelog.cpp: CHANGELOG.md as release-please
// writes it, read into the lines the About tab shows.
#include "check.h"
#include <cstdio>
#include <string>

#include "util/changelog.hpp"

using changelog::Line;

static const char* MD =
    "# Changelog\n"
    "\n"
    "## [1.1.0](https://github.com/jigsawfr/playshelf/compare/v1.0.0...v1.1.0) (2026-10-10)\n"
    "\n"
    "\n"
    "### \xF0\x9F\x9A\x80 Features\n"   // 🚀
    "\n"
    "* **ui:** an About tab with the changelog ([#33](https://github.com/x/y/issues/33)) ([0123abc](https://github.com/x/y/commit/0123abcdef))\n"
    "* a long item\n"
    "  that goes on\n"
    "  * a nested one\n"
    "\n"
    "### \xE2\x9A\xA0\xEF\xB8\x8F Changes\n"   // ⚠️
    "\n"
    "- uses `romfs` and [borealis](https://github.com/xfangfang/borealis)\n"
    "\r\n"
    "## 1.0.0 (2026-09-01)\n"
    "\n"
    "First release.\n"
    "\n"
    "## Upstream history\n"
    "\n"
    "### Pctl Manager v3.0.0\n";

int main()
{
    const auto all = changelog::parse(MD, 0);
    CHECK(all.size() == 11);
    CHECK(all[0].kind == Line::Release && all[0].text == "1.1.0 · 2026-10-10");
    CHECK(all[1].kind == Line::Section && all[1].text == "Features" && all[1].icon == "1f680");
    CHECK(all[2].kind == Line::Item && all[2].title == "ui" && all[2].text == "an About tab with the changelog" && all[2].depth == 0);
    CHECK(all[2].icon.empty() && all[3].title.empty());
    CHECK(all[3].kind == Line::Item && all[3].text == "a long item that goes on");
    CHECK(all[4].kind == Line::Item && all[4].text == "a nested one" && all[4].depth == 1);
    CHECK(all[5].kind == Line::Section && all[5].text == "Changes" && all[5].icon == "26a0");
    CHECK(all[10].icon.empty());
    CHECK(all[6].kind == Line::Item && all[6].text == "uses romfs and borealis");
    CHECK(all[7].kind == Line::Release && all[7].text == "1.0.0 · 2026-09-01");
    CHECK(all[8].kind == Line::Text && all[8].text == "First release.");
    CHECK(all[9].kind == Line::Release && all[9].text == "Upstream history");
    CHECK(all[10].kind == Line::Section && all[10].text == "Pctl Manager v3.0.0");

    // At most two releases: the upstream history is left out.
    const auto two = changelog::parse(MD, 2);
    CHECK(two.size() == 9 && two.back().text == "First release.");

    // Text that only looks like a link or a reference stays.
    const auto plain = changelog::parse("* see [docs] and (abc) and ([not a ref](u))\n", 0);
    CHECK(plain.size() == 1 && plain[0].text == "see [docs] and (abc) and (not a ref)");

    // Only the running version's notes.
    std::string date;
    const auto notes = changelog::release_notes(MD, "1.1.0", &date);
    CHECK(date == "2026-10-10");
    CHECK(notes.size() == 6 && notes[0].text == "Features" && notes.back().text == "uses romfs and borealis");
    const auto first = changelog::release_notes(MD, "v1.0.0", &date);
    CHECK(date == "2026-09-01" && first.size() == 1 && first[0].text == "First release.");
    date = "unchanged";
    CHECK(changelog::release_notes(MD, "1.2.0", &date).empty() && date == "unchanged");
    CHECK(changelog::release_notes(MD, "1.1", nullptr).empty());
    CHECK(changelog::release_notes(MD, "", nullptr).empty());

    // What changed since a version: the newer releases, headings included.
    const auto since = changelog::newer_than(MD, "1.0.0");
    CHECK(since.size() == 7 && since[0].kind == Line::Release && since[0].text == "1.1.0 · 2026-10-10" &&
          since.back().text == "uses romfs and borealis");
    const auto older = changelog::newer_than(MD, "0.9.0");
    CHECK(older.size() == 9 && older[7].text == "1.0.0 · 2026-09-01" && older[8].text == "First release.");
    CHECK(changelog::newer_than(MD, "1.1.0").empty() && changelog::newer_than(MD, "2.0.0").empty());

    // A release's hand-written summary (.github/release-notes/<version>.md):
    // the highlights About shows, without a "## " heading of its own.
    const auto summary = changelog::parse("### \xF0\x9F\x8C\x9F Highlights\n"   // 🌟
                                          "\n"
                                          "The first release.\n"
                                          "\n"
                                          "* **Play-time limit**: one per day, in\n"
                                          "  `sd:/switch/playshelf/`.\n", 0);
    CHECK(summary.size() == 3);
    CHECK(summary[0].kind == Line::Section && summary[0].text == "Highlights" && summary[0].icon == "1f31f");
    CHECK(summary[1].kind == Line::Text && summary[1].text == "The first release.");
    CHECK(summary[2].kind == Line::Item && summary[2].title == "Play-time limit" &&
          summary[2].text == "one per day, in sd:/switch/playshelf/.");

    // The bold lead-ins as titles, either way round the colon, and the emoji
    // an item opens on as its icon.
    const auto hl = changelog::parse("### Highlights\n\n"
                                          "* \xF0\x9F\x93\x8A **Activity**: per game,\n  per day.\n"
                                          "* \xE2\x8F\xB1\xEF\xB8\x8F **Safe:** backed up\n"
                                          "* a **bold** word\n"
                                          "* \xF0\x9F\x8C\x8D **Every language**, two themes\n"
                                          "  * **nested**: kept as is\n", 0);
    CHECK(hl.size() == 6);
    CHECK(hl[1].title == "Activity" && hl[1].text == "per game, per day." && hl[1].icon == "1f4ca");
    CHECK(hl[2].title == "Safe" && hl[2].text == "backed up" && hl[2].icon == "23f1");
    CHECK(hl[3].title.empty() && hl[3].text == "a bold word" && hl[3].icon.empty());
    CHECK(hl[4].title.empty() && hl[4].text == "Every language, two themes" && hl[4].icon == "1f30d");
    CHECK(hl[5].title.empty() && hl[5].text == "nested: kept as is" && hl[5].depth == 1 && hl[5].icon.empty());

    // A release's notes as the release job lays them out: the summary, the
    // commit list folded in <details>, then an install section for a computer.
    const char* release =
        "## [1.2.0](https://github.com/x/y/compare/v1.1.0...v1.2.0) (2026-11-01)\n"
        "\n"
        "### \xF0\x9F\x8C\x9F Highlights\n"   // 🌟
        "\n"
        "A short release.\n"
        "\n"
        "<details>\n"
        "<summary><b>Every change in 1.2.0</b> (1 change)</summary>\n"
        "\n"
        "### Bug Fixes\n"
        "\n"
        "* a fix ([#9](https://github.com/x/y/issues/9))\n"
        "\n"
        "</details>\n"
        "\n"
        "## Install\n"
        "\n"
        "| File | What it is for |\n";
    const auto rel = changelog::release_notes(release, "1.2.0", &date);
    CHECK(date == "2026-11-01" && rel.size() == 4);
    CHECK(rel[0].kind == Line::Section && rel[0].text == "Highlights");
    CHECK(rel[1].kind == Line::Text && rel[1].text == "A short release.");
    CHECK(rel[2].kind == Line::Section && rel[2].text == "Bug Fixes");
    CHECK(rel[3].kind == Line::Item && rel[3].text == "a fix");

    // The release page as the release job lays it out now: the heading with
    // a "·", the figures and rules in HTML, two-line bullets, a GitHub-only
    // block hidden here, a line for the console only, comments skipped.
    const char* page =
        "## [1.3.0](https://github.com/x/y/compare/v1.2.0...v1.3.0) \xC2\xB7 2026-12-01\n"   // ·
        "\n"
        "<p>\xE2\xAC\x87\xEF\xB8\x8F <a href=\"u\"><b>Download</b></a></p>\n"
        "\n"
        "<hr>\n"
        "\n"
        "### \xF0\x9F\x8C\x9F Highlights\n"
        "\n"
        "* \xF0\x9F\x93\x8A **Activity**:  \n"
        "  A month at a glance.\n"
        "\n"
        "* **Clock**: on time\n"
        "\n"
        "<!-- console:hide -->\n"
        "> [!IMPORTANT]\n"
        "> **Moved?** Read this.\n"
        "<!-- /console -->\n"
        "<!-- console: Moved? Read this. -->\n"
        "<!-- a note for the maintainers,\n"
        "     on two lines -->\n"
        "<!-- console:only\n"
        "* only here\n"
        "-->\n"
        "<!-- console:hide -->\n"
        "### What changed\n"
        "<!-- /console -->\n"
        "<details>\n"
        "<summary><b>Every change</b> (1 change)</summary>\n"
        "\n"
        "### Bug Fixes\n"
        "\n"
        "* a fix\n"
        "\n"
        "</details>\n"
        "\n"
        "## Install\n";
    const auto pg = changelog::release_notes(page, "1.3.0", &date);
    CHECK(date == "2026-12-01" && pg.size() == 7);
    CHECK(pg[0].kind == Line::Section && pg[0].text == "Highlights");
    CHECK(pg[1].kind == Line::Item && pg[1].title == "Activity" && pg[1].text == "A month at a glance." && pg[1].icon == "1f4ca");
    CHECK(pg[2].kind == Line::Item && pg[2].title == "Clock" && pg[2].text == "on time");
    CHECK(pg[3].kind == Line::Text && pg[3].text == "Moved? Read this.");
    CHECK(pg[4].kind == Line::Item && pg[4].text == "only here");
    CHECK(pg[5].kind == Line::Section && pg[5].text == "Bug Fixes");
    CHECK(pg[6].kind == Line::Item && pg[6].text == "a fix");
    // The summary: the highlights, up to release-please's first list.
    CHECK(changelog::summary_end(pg) == 5);
    CHECK(changelog::summary_end(changelog::release_notes(MD, "1.1.0")) == 0);
    CHECK(changelog::summary_end(changelog::parse("### Highlights\n\n* one\n", 0)) == 2);
    CHECK(changelog::summary_end({}) == 0);
    // An unclosed hide block hides the rest; an unclosed comment too.
    CHECK(changelog::parse("<!-- console:hide -->\n* gone\n", 0).empty());
    CHECK(changelog::parse("<!-- open\n* gone\n", 0).empty());

    // A bullet with nothing left once cleaned is dropped, and an indented line
    // after it continues nothing (the fuzzer's find: it read past an empty list).
    const auto empty_item = changelog::parse("* **\n  continued\n", 0);
    CHECK(empty_item.size() == 1 && empty_item[0].kind == Line::Text && empty_item[0].text == "continued");
    const auto after_item = changelog::parse("* one\n* **\n  two\n", 0);
    CHECK(after_item.size() == 2 && after_item[0].text == "one" && after_item[1].kind == Line::Text);

    CHECK(changelog::parse("", 0).empty());
    CHECK(changelog::parse("# Changelog\n", 0).empty());

    // A keycap emoji (1, U+FE0F, U+20E3): the digit stays, its marks go, as in
    // the Markdown view (util/emoji).
    const auto keycap = changelog::parse("## [1.0.0](u) (2026-01-01)\n\n* 1\xEF\xB8\x8F\xE2\x83\xA3 first step\n", 0);
    CHECK(keycap.size() == 2 && keycap[1].kind == Line::Item && keycap[1].text == "1 first step");

    return CHECK_DONE("changelog parsing, links, references and emoji assertions passed");
}
