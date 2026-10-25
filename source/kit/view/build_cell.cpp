// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/build_cell.hpp"

#include "action/dev_build_flow.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
// The kind of change: a word and a colour (the common types named,
// others as they are written).
BuildTag type_tag(const std::string& type)
{
    struct Known { const char* type; const char* key; int color; };
    // color: 0 accent, 1 amber, 2 blue, 3 purple, 4 red, 5 grey
    static const Known KNOWN[] = {
        { "feat", "playshelf/dev_build/type_feat", 0 },   { "fix", "playshelf/dev_build/type_fix", 1 },
        { "docs", "playshelf/dev_build/type_docs", 2 },   { "perf", "playshelf/dev_build/type_perf", 3 },
        { "revert", "playshelf/dev_build/type_revert", 4 }, { "refactor", "playshelf/dev_build/type_refactor", 5 },
        { "test", "playshelf/dev_build/type_test", 5 },   { "chore", "playshelf/dev_build/type_chore", 5 },
    };
    int color = 5;
    std::string text = type;
    for (const Known& k : KNOWN)
        if (type == k.type) {
            text = brls::getStr(k.key);
            color = k.color;
        }
    const NVGcolor colors[] = { cadran::kit::color("cadran/teal"), cadran::kit::color("cadran/amber"), cadran::kit::color("cadran/blue"), cadran::kit::color("cadran/violet"),
                                cadran::kit::color("cadran/coral"), cadran::kit::color("cadran/text3") };
    return { text, colors[color] };
}
}   // namespace

std::vector<BuildTag> build_tags(const dev_builds::Build& b, bool newer)
{
    std::vector<BuildTag> tags;
    const dev_builds::Subject s = dev_builds::parse_subject(b.title);
    if (b.kind == dev_builds::Kind::PullRequest) tags.push_back({ "#" + std::to_string(b.pr), cadran::kit::color("cadran/teal") });
    if (b.kind != dev_builds::Kind::Release) {
        if (!s.type.empty()) tags.push_back(type_tag(s.type));
        if (!s.scope.empty()) tags.push_back({ s.scope, cadran::kit::color("cadran/text3") });
        if (s.breaking) tags.push_back({ "playshelf/dev_build/breaking"_i18n, cadran::kit::color("cadran/coral") });
        if (s.pr && b.kind != dev_builds::Kind::PullRequest) tags.push_back({ "#" + std::to_string(s.pr), cadran::kit::color("cadran/text3") });
    }
    if (b.prerelease) tags.push_back({ "playshelf/dev_build/kind_prerelease"_i18n, cadran::kit::color("cadran/amber") });
    switch (b.ci) {
        case dev_builds::Ci::Passed: tags.push_back({ "playshelf/dev_build/tag_ci_passed"_i18n, cadran::kit::color("cadran/teal") }); break;
        case dev_builds::Ci::Failed: tags.push_back({ "playshelf/dev_build/tag_ci_failed"_i18n, cadran::kit::color("cadran/coral") }); break;
        case dev_builds::Ci::Running: tags.push_back({ "playshelf/dev_build/tag_ci_running"_i18n, cadran::kit::color("cadran/amber") }); break;
        case dev_builds::Ci::Unknown: break;
    }
    // Installed or new: an icon, no word (the tabs' count of new ones has the same sparkle).
    if (dev_build_flow::installed(b)) tags.push_back({ "", cadran::kit::color("cadran/teal"), "check" });
    else if (newer) tags.push_back({ "", cadran::kit::color("cadran/teal"), "sparkle" });
    return tags;
}

std::string build_title(const dev_builds::Build& b)
{
    switch (b.kind) {
        case dev_builds::Kind::Release: return brls::getStr("playshelf/dev_build/release", b.version);
        case dev_builds::Kind::PullRequest:
        case dev_builds::Kind::Main:
        case dev_builds::Kind::Commit: return dev_build_flow::subject(b);
    }
    return b.commit;
}

brls::Box* tag_view(const BuildTag& tag, float font_size)
{
    // A pill of the boards, tinted with its ink.
    const float height = font_size + 10;
    auto* box = new brls::Box(brls::Axis::ROW);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setHeight(height);
    box->setBackgroundColor(nvgTransRGBA(tag.color, 30));
    box->setBorderColor(nvgTransRGBA(tag.color, 90));
    box->setBorderThickness(1);
    box->setCornerRadius(height / 2);
    box->setPadding(0, 10, 0, 10);
    box->setMarginRight(8);
    box->setShrink(0.0f);
    if (tag.icon) {
        CadranIcon* glyph = new CadranIcon();
        glyph->setIcon(tag.icon);
        glyph->setColor(tag.color);
        glyph->setWidth(font_size - 2);
        glyph->setHeight(font_size - 2);
        if (!tag.text.empty()) glyph->setMarginRight(6);
        box->addView(glyph);
    }
    if (tag.text.empty()) return box;
    auto* l = new brls::Label();
    l->setSingleLine(true);
    l->setFontSize(font_size);
    l->setTextColor(tag.color);
    l->setText(tag.text);
    box->addView(l);
    return box;
}

BuildCell::BuildCell()
{
    // A list cell's separator and focus (RecyclerCell, as DetailCell), two lines high.
    this->setAxis(brls::Axis::ROW);
    this->setHeight(brls::View::AUTO);
    this->setFocusable(true);
    this->setAlignItems(brls::AlignItems::CENTER);
    const float indent = brls::getStyle()["brls/listitem/descriptionIndent"];
    this->setPadding(10, indent, 10, indent);

    auto* left = new brls::Box(brls::Axis::COLUMN);
    left->setGrow(1.0f);
    left->setShrink(1.0f);
    left->setMarginRight(indent);
    this->title = new brls::Label();
    this->title->setSingleLine(true);
    this->title->setFontSize(21);
    left->addView(this->title);
    this->tags = new brls::Box(brls::Axis::ROW);
    this->tags->setMarginTop(6);
    left->addView(this->tags);
    this->addView(left);

    auto* right = new brls::Box(brls::Axis::COLUMN);
    right->setAlignItems(brls::AlignItems::FLEX_END);
    right->setShrink(0.0f);
    this->age = new brls::Label();
    this->age->setSingleLine(true);
    this->age->setFontSize(18);
    this->age->setTextColor(cadran::kit::color("cadran/text2"));
    this->age->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    right->addView(this->age);
    this->commit = new brls::Label();
    this->commit->setSingleLine(true);
    this->commit->setFontSize(18);
    this->commit->setTextColor(cadran::kit::color("cadran/text3"));
    this->commit->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    this->commit->setMarginTop(4);
    right->addView(this->commit);
    this->addView(right);
}

void BuildCell::set(const dev_builds::Build& b, bool newer)
{
    this->title->setText(build_title(b));
    this->tags->clearViews();
    for (const BuildTag& t : build_tags(b, newer)) this->tags->addView(tag_view(t));
    this->age->setText(dev_build_flow::age_text(b.date));
    // A release: its date; anything else: its commit.
    this->commit->setText(b.kind == dev_builds::Kind::Release ? b.date.substr(0, 10) : b.commit);
}
