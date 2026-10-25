// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/markdown_view.hpp"

#include <string>

#include "ui/cadran_list.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"
#include "util/text.hpp"

using namespace brls::literals;

namespace markdown_view
{
namespace kit = cadran::kit;

namespace
{
// A label that takes the rest of its row and wraps there.
brls::Label* growing(const std::string& text, float size, NVGcolor color)
{
    brls::Label* l = kit::label(text, size);
    l->setTextColor(color);
    l->setGrow(1.0f);
    l->setShrink(1.0f);
    return l;
}

// The emoji `hex` from the Twemoji set in img/emoji/, `size` wide; null
// when it is not one of them. Only hex digits reach the path.
brls::Image* icon(const std::string& hex, float size)
{
    if (hex.empty() || hex.size() > 6 || hex.find_first_not_of("0123456789abcdef") != std::string::npos) return nullptr;
    if (!paths::file_exists(std::string(BRLS_ASSET("img/emoji/")) + hex + ".png")) return nullptr;
    auto* image = new brls::Image();
    image->setScalingType(brls::ImageScalingType::FIT);
    image->setImageFromRes("img/emoji/" + hex + ".png");
    image->setWidth(size);
    image->setHeight(size);
    image->setShrink(0.0f);
    return image;
}

// A heading as the boards': the first two levels in the display face, the
// third as a kicker (capitals, second ink), the others as a line's text.
brls::View* heading(const markdown::Block& b, bool first)
{
    const int level = b.level >= 1 && b.level <= 6 ? b.level : 6;
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setMarginTop(first ? 0 : (level <= 2 ? 26 : 18));
    row->setMarginBottom(level <= 2 ? 10 : 6);
    const float size = level == 1 ? 22 : level == 2 ? 20 : level == 3 ? kit::TEXT_KICKER : 18;
    if (brls::Image* image = icon(b.icon, size + 4)) {
        image->setMarginRight(10);
        row->addView(image);
    }
    brls::Label* text = level <= 2   ? kit::display(b.text, size)
                        : level == 3 ? kit::label(cadran::upper(b.text), size, "cadran/text2")
                                     : kit::label(b.text, size, "cadran/text");
    text->setGrow(1.0f);
    text->setShrink(1.0f);
    row->addView(text);
    return row;
}

// A task's box: ticked (filled, a white mark inside) or not (an outline).
brls::View* checkbox(bool done)
{
    auto* box = new brls::Box(brls::Axis::ROW);
    box->setWidth(20);
    box->setHeight(20);
    box->setShrink(0.0f);
    box->setCornerRadius(4);
    box->setBorderThickness(2);
    box->setBorderColor(done ? kit::color("cadran/teal_fill") : kit::color("cadran/text3"));
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setMarginTop(4);
    if (done) {
        box->setBackgroundColor(kit::color("cadran/teal_fill"));
        auto* mark = new brls::Rectangle(kit::color("cadran/navy"));
        mark->setWidth(8);
        mark->setHeight(8);
        mark->setCornerRadius(2);
        box->addView(mark);
    }
    return box;
}

brls::View* item(const markdown::Block& b)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::FLEX_START);
    row->setMarginLeft(8 + 26 * b.level);
    row->setMarginTop(6);
    brls::View* marker = nullptr;
    if (b.task >= 0) marker = checkbox(b.task == 1);
    else if (b.number > 0) marker = kit::label(std::to_string(b.number) + ".", 18, "cadran/teal");
    else if (!(marker = icon(b.icon, 20))) marker = kit::label(b.level == 0 ? "•" : "–", 18, "cadran/teal");
    marker->setMinWidth(22);
    marker->setMarginRight(10);
    marker->setShrink(0.0f);
    row->addView(marker);
    // A task done reads quieter, as GitHub greys it.
    row->addView(growing(b.text, 18, b.task == 1 ? kit::color("cadran/text3") : kit::color("cadran/text")));
    return row;
}

brls::View* code(const markdown::Block& b)
{
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setBackgroundColor(kit::color("cadran/surf2"));
    box->setBorderColor(kit::color("cadran/line"));
    box->setBorderThickness(1);
    box->setCornerRadius(kit::RADIUS_ROW);
    box->setPadding(12, 18, 12, 18);
    box->setMarginTop(8);
    box->setMarginBottom(4);
    box->addView(kit::label(text::join(b.lines, "\n"), kit::TEXT_BODY, "cadran/text"));
    return box;
}

brls::View* quote(const markdown::Block& b)
{
    // GitHub's alerts: their colour and their title, on the second surface
    // as the boards' notes.
    NVGcolor color = kit::color("cadran/text3");
    std::string title;
    if (b.alert == "note") color = kit::color("cadran/blue"), title = "playshelf/markdown/note"_i18n;
    else if (b.alert == "tip") color = kit::color("cadran/teal"), title = "playshelf/markdown/tip"_i18n;
    else if (b.alert == "important") color = kit::color("cadran/violet"), title = "playshelf/markdown/important"_i18n;
    else if (b.alert == "warning") color = kit::color("cadran/amber"), title = "playshelf/markdown/warning"_i18n;
    else if (b.alert == "caution") color = kit::color("cadran/coral"), title = "playshelf/markdown/caution"_i18n;
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::STRETCH);
    row->setMarginTop(8);
    if (!title.empty()) {
        row->setBackgroundColor(kit::color("cadran/surf2"));
        row->setCornerRadius(kit::RADIUS_ROW);
        row->setPaddingLeft(10);
    }
    auto* bar = new brls::Rectangle(color);
    bar->setWidth(4);
    bar->setCornerRadius(2);
    bar->setMarginRight(14);
    row->addView(bar);
    auto* body = new brls::Box(brls::Axis::COLUMN);
    body->setGrow(1.0f);
    body->setShrink(1.0f);
    body->setPadding(6, 8, 6, 0);
    if (!title.empty()) {
        brls::Label* t = kit::label(cadran::upper(title), kit::TEXT_META);
        t->setTextColor(color);
        t->setMarginBottom(4);
        body->addView(t);
    }
    if (!b.text.empty()) body->addView(kit::label(b.text, 18, title.empty() ? "cadran/text2" : "cadran/text"));
    row->addView(body);
    return row;
}

brls::View* table(const markdown::Block& b)
{
    size_t columns = 0;
    for (const auto& r : b.rows) columns = std::max(columns, r.size());
    auto* grid = new brls::Box(brls::Axis::COLUMN);
    grid->setMarginTop(8);
    grid->setBorderThickness(1);
    grid->setBorderColor(kit::color("cadran/line"));
    grid->setCornerRadius(kit::RADIUS_ROW);
    grid->setBackgroundColor(kit::color("cadran/surf"));
    for (size_t r = 0; r < b.rows.size(); r++) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setAlignItems(brls::AlignItems::STRETCH);
        if (r + 1 < b.rows.size()) {
            row->setLineBottom(1);
            row->setLineColor(kit::color("cadran/line"));
        }
        for (size_t c = 0; c < columns; c++) {
            const std::string text = c < b.rows[r].size() ? b.rows[r][c] : "";
            auto* cell = new brls::Box(brls::Axis::ROW);
            cell->setWidthPercentage(100.0f / (float)columns);
            cell->setPadding(8, 14, 8, 14);
            cell->addView(growing(text, kit::TEXT_BODY, r == 0 ? kit::color("cadran/text2") : kit::color("cadran/text")));
            row->addView(cell);
        }
        grid->addView(row);
    }
    return grid;
}
}   // namespace

void fill(brls::Box* box, const std::vector<markdown::Block>& blocks)
{
    bool first = true;
    for (const markdown::Block& b : blocks) {
        brls::View* view = nullptr;
        switch (b.kind) {
            case markdown::Block::Heading: view = heading(b, first); break;
            case markdown::Block::Paragraph:
                view = kit::label(b.text, 18, "cadran/text");
                view->setMarginTop(8);
                break;
            case markdown::Block::Item: view = item(b); break;
            case markdown::Block::Code: view = code(b); break;
            case markdown::Block::Quote: view = quote(b); break;
            case markdown::Block::Rule: {
                auto* rule = new brls::Rectangle(kit::color("cadran/line"));
                rule->setHeight(1);
                rule->setMarginTop(14);
                rule->setMarginBottom(6);
                view = rule;
                break;
            }
            case markdown::Block::Table: view = table(b); break;
        }
        box->addView(view);
        first = false;
    }
}

}   // namespace markdown_view
