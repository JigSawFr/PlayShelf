// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/release_notes.hpp"

#include "app.hpp"
#include "ui/cadran_list.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"
#include "util/text.hpp"

namespace release_notes
{
namespace
{
// The emoji `hex` ("1f680") as an image `size` pixels wide, from the Twemoji
// set in img/emoji/; nullptr when it is not one of them.
brls::Image* icon(const std::string& hex, float size)
{
    if (hex.empty() || !paths::file_exists(std::string(BRLS_ASSET("img/emoji/")) + hex + ".png")) return nullptr;
    auto* image = new brls::Image();
    image->setScalingType(brls::ImageScalingType::FIT);
    image->setImageFromRes("img/emoji/" + hex + ".png");
    image->setWidth(size);
    image->setHeight(size);
    return image;
}

namespace kit = cadran::kit;

// "🚀 Features": its icon and its title in the display face, a card of its
// items to follow.
brls::View* section(const changelog::Line& l, bool first)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setMarginTop(first ? 0 : 26);
    row->setMarginBottom(10);
    if (brls::Image* image = icon(l.icon, 24)) {
        image->setMarginRight(10);
        row->addView(image);
    }
    row->addView(kit::display(l.text, 20));
    return row;
}

// "activity" before an item of the full list: a small pill, as the boards'.
brls::View* tag(const std::string& text)
{
    auto* box = new brls::Box(brls::Axis::ROW);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setBackgroundColor(kit::color("cadran/surf2"));
    box->setBorderColor(kit::color("cadran/line"));
    box->setBorderThickness(1);
    box->setHeight(24);
    box->setCornerRadius(12);
    box->setPadding(0, 10, 0, 10);
    box->setMarginRight(10);
    box->setShrink(0.0f);
    brls::Label* label = kit::label(text, kit::TEXT_META, "cadran/text2");
    label->setSingleLine(true);
    box->addView(label);
    return box;
}

// One highlight: a card of the boards, its icon on the left (a teal bar
// when it has none), its title over its text; in half the row (`left`: the
// first half), the gap between the two split over both.
brls::Box* card(const changelog::Line& l, bool left)
{
    auto* half = new brls::Box(brls::Axis::ROW);
    half->setWidthPercentage(50);
    half->setAlignItems(brls::AlignItems::STRETCH);
    if (left) half->setPaddingRight(6);
    else half->setPaddingLeft(6);
    brls::Box* box = kit::card(false, true);
    box->setAxis(brls::Axis::ROW);
    box->setAlignItems(brls::AlignItems::FLEX_START);
    box->setPadding(16, 20, 16, 18);
    if (brls::Image* image = icon(l.icon, 36)) {
        image->setMarginRight(14);
        box->addView(image);
    } else {
        box->setAlignItems(brls::AlignItems::STRETCH);
        auto* bar = new brls::Rectangle(kit::color("cadran/teal"));
        bar->setWidth(4);
        bar->setCornerRadius(2);
        bar->setMarginRight(14);
        box->addView(bar);
    }
    auto* body = new brls::Box(brls::Axis::COLUMN);
    body->setGrow(1.0f);
    body->setShrink(1.0f);
    if (l.title.empty()) {
        body->addView(kit::label(l.text, kit::TEXT_LEAD, "cadran/text"));   // no lead-in: the line is the title
    } else {
        brls::Label* title = kit::display(l.title, kit::TEXT_LEAD);
        title->setMarginBottom(4);
        body->addView(title);
        body->addView(kit::label(text::capitalise_ascii(l.text), kit::TEXT_BODY, "cadran/text2"));
    }
    box->addView(body);
    half->addView(box);
    return half;
}
}   // namespace

std::vector<changelog::Line> current(std::string* date)
{
    std::string md;
    paths::read_file(BRLS_ASSET("CHANGELOG.md"), md);
    return changelog::release_notes(md, app::version(), date);
}

std::vector<changelog::Line> highlights()
{
    std::string md;
    if (!paths::read_file(std::string(BRLS_ASSET("release-notes/")) + app::version() + ".md", md)) return {};
    return changelog::parse(md, 0);
}

std::vector<changelog::Line> bundled()
{
    std::vector<changelog::Line> lines = highlights();
    for (changelog::Line& l : current()) lines.push_back(std::move(l));
    return lines;
}

void fill(brls::Box* box, const std::vector<changelog::Line>& lines)
{
    bool first = true;
    brls::Box* items = nullptr;   // the card the next items join, as a list's rows
    for (const changelog::Line& l : lines) {
        brls::View* view = nullptr;
        switch (l.kind) {
            case changelog::Line::Release:   // not under one version's heading
            case changelog::Line::Section:
                view = section(l, first);
                items = nullptr;
                break;
            case changelog::Line::Item: {
                auto* row = new brls::Box(brls::Axis::ROW);
                row->setAlignItems(brls::AlignItems::FLEX_START);   // bullet and tag on the first line
                row->setPadding(12, 22, 12, 20 + 24 * l.depth);
                brls::View* bullet = icon(l.icon, 20);
                if (!bullet) {
                    bullet = kit::label(l.depth ? "–" : "•", 19, "cadran/teal");
                    bullet->setWidth(20);
                }
                bullet->setMarginRight(10);
                bullet->setShrink(0.0f);
                row->addView(bullet);
                if (!l.title.empty()) row->addView(tag(l.title));
                brls::Label* text = kit::label(text::capitalise_ascii(l.text), 18, "cadran/text");
                text->setGrow(1.0f);
                text->setShrink(1.0f);
                row->addView(text);
                if (!items) {
                    items = new cadran::Card();
                    box->addView(items);
                }
                items->addView(row);
                first = false;
                continue;
            }
            case changelog::Line::Text:
                view = kit::label(l.text, kit::TEXT_NOTE, "cadran/text3");
                view->setMarginTop(8);
                items = nullptr;
                break;
        }
        box->addView(view);
        first = false;
    }
}

void fill_highlights(brls::Box* box, const std::vector<changelog::Line>& lines)
{
    bool       after_items = false;
    brls::Box* row         = nullptr;   // the row with room for one more card
    for (const changelog::Line& l : lines) {
        if (l.kind == changelog::Line::Item && l.depth == 0) {
            if (!row) {
                row = new brls::Box(brls::Axis::ROW);
                row->setAlignItems(brls::AlignItems::STRETCH);
                row->setMarginTop(12);
                box->addView(row);
                row->addView(card(l, true));
            } else {
                row->addView(card(l, false));
                row = nullptr;
            }
            after_items = true;
            continue;
        }
        if (l.kind == changelog::Line::Item) continue;   // a nested item: not on a card
        brls::View* view = nullptr;
        if (l.kind == changelog::Line::Text) {
            view = after_items ? kit::label(l.text, kit::TEXT_NOTE, "cadran/text3") : kit::label(l.text, kit::TEXT_LEAD, "cadran/text2");
            view->setMarginTop(after_items ? 16 : 2);
        } else {
            view = section(l, box->getChildren().empty());
        }
        box->addView(view);
        row = nullptr;
    }
}

void fill_release(brls::Box* box, const std::vector<changelog::Line>& lines)
{
    const size_t end = changelog::summary_end(lines);
    if (end == 0) {
        fill(box, lines);
        return;
    }
    fill_highlights(box, { lines.begin(), lines.begin() + (long)end });
    if (end == lines.size()) return;
    auto* lists = new brls::Box(brls::Axis::COLUMN);
    lists->setAlignItems(brls::AlignItems::STRETCH);
    lists->setMarginTop(28);
    fill(lists, { lines.begin() + (long)end, lines.end() });
    box->addView(lists);
}

}   // namespace release_notes
