// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/dev_build_activity.hpp"

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_modal.hpp>
#include <borealis/cadran/util/markdown.hpp>
#include <borealis/cadran/view/markdown_view.hpp>

#include "action/dev_build_flow.hpp"
#include "action/rollback_flow.hpp"
#include "app.hpp"
#include "ui/ui.hpp"
#include "util/changelog.hpp"
#include "view/build_cell.hpp"
#include "view/release_notes.hpp"

using namespace brls::literals;

void DevBuildActivity::show(const dev_builds::Build& build, bool newer)
{
    brls::Application::pushActivity(new DevBuildActivity(build, newer));
}

void DevBuildActivity::add_fact(const std::string& name, const std::string& value, const char* color)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::FLEX_START);
    row->setPadding(10, 0, 10, 0);
    if (!facts->getChildren().empty()) {
        row->setLineTop(1);
        row->setThemeColor("lineColor", "cadran/line");
    }
    brls::Label* n = cadran::kit::label(name, cadran::kit::TEXT_BODY, "cadran/text2");
    n->setWidth(230);
    n->setShrink(0.0f);
    row->addView(n);
    brls::Label* v = cadran::kit::label(value, cadran::kit::TEXT_BODY, color);
    v->setGrow(1.0f);
    v->setShrink(1.0f);
    row->addView(v);
    facts->addView(row);
}

std::string DevBuildActivity::page() const
{
    const dev_builds::Build& b = this->build;
    if (!b.page.empty()) return b.page;
    const std::string repo = app::repo_url();
    switch (b.kind) {
        case dev_builds::Kind::Release: return repo + "/releases/tag/v" + b.version;   // a version: safe_text::is_version
        case dev_builds::Kind::PullRequest: return repo + "/pull/" + std::to_string(b.pr);
        case dev_builds::Kind::Main:
        case dev_builds::Kind::Commit: return b.commit.empty() ? "" : repo + "/commit/" + b.commit;   // 7 hex digits
    }
    return "";
}

void DevBuildActivity::show_page()
{
    const std::string url = this->page();
    if (!url.empty()) ui::qr_dialog("playshelf/dev_build/open_github_body"_i18n, url);
}

void DevBuildActivity::onContentAvailable()
{
    const dev_builds::Build& b = this->build;
    headline->setSingleLine(false);
    warning->setSingleLine(false);
    // The facts as a card of the boards, the buttons as their pills.
    facts->setThemeColor("backgroundColor", "cadran/surf");
    facts->setThemeColor("borderColor", "cadran/line");
    facts->setBorderThickness(1);
    facts->setCornerRadius(cadran::kit::RADIUS_CARD);
    cadran::style_button(install.getView(), cadran::Emphasis::PRIMARY);
    cadran::style_button(github.getView());

    // Tags: its kind, then the list's (kind of change, scope, pull request,
    // checks, pre-release, installed or new).
    const char* kind_color = "cadran/teal";
    switch (b.kind) {
        case dev_builds::Kind::Release: tags->addView(tag_view({ "playshelf/dev_build/kind_release"_i18n, kind_color }, 18)); break;
        case dev_builds::Kind::Main: tags->addView(tag_view({ "main", kind_color }, 18)); break;
        case dev_builds::Kind::PullRequest: break;   // its number leads the tags already
        case dev_builds::Kind::Commit: tags->addView(tag_view({ "playshelf/dev_build/kind_commit"_i18n, kind_color }, 18)); break;
    }
    for (const BuildTag& t : build_tags(b, this->newer)) tags->addView(tag_view(t, 18));
    headline->setText(build_title(b));

    // The facts.
    std::string built = dev_builds::short_date(b.date);
    const std::string age = dev_build_flow::age_text(b.date);
    if (!age.empty()) built += " · " + age;
    add_fact("playshelf/dev_build/fact_built"_i18n, built, "cadran/text");
    add_fact("playshelf/dev_build/fact_size"_i18n,
             brls::getStr("playshelf/dev_build/size_mb", fmt::format("{:.1f}", (double)b.size / (1024.0 * 1024.0))),
             "cadran/text");
    if (!b.commit.empty()) add_fact("playshelf/dev_build/fact_commit"_i18n, b.commit, "cadran/text");
    if (b.kind == dev_builds::Kind::Commit && !b.branch.empty())
        add_fact("playshelf/dev_build/fact_branch"_i18n, b.branch, "cadran/text");
    if (b.sha256.empty()) add_fact("playshelf/dev_build/fact_checksum"_i18n, "playshelf/dev_build/no_digest"_i18n, "cadran/amber");
    else add_fact("playshelf/dev_build/fact_checksum"_i18n, b.sha256.substr(0, 16) + "…", "cadran/text");
    if (!b.expires.empty()) {
        const int days = std::max(0, dev_builds::days_left(b.expires, dev_build_flow::now()));
        add_fact("playshelf/dev_build/fact_expires"_i18n, brls::getStr("playshelf/dev_build/expires_in", days),
                 days < 7 ? "cadran/amber" : "cadran/text");
    }

    const std::string running = rollback_flow::running_label();
    std::string note = brls::getStr("playshelf/dev_build/replaces", running);
    if (b.kind != dev_builds::Kind::Release) note += " " + "playshelf/dev_build/untested"_i18n;
    // Built, but its tests and checks have not finished: they may still fail.
    if (b.ci == dev_builds::Ci::Running) note += "\n\n" + "playshelf/dev_build/ci_running_note"_i18n;
    warning->setText(note);

    install->registerClickAction([b](brls::View*) {
        dev_build_flow::install_now(b);
        return true;
    });
    github->registerClickAction([this](brls::View*) {
        this->show_page();
        return true;
    });
    ui::set_visible(github.getView(), !this->page().empty());
    // No checksum: it cannot be installed (the facts say why), so Install is
    // not offered, and See on GitHub takes the focus. Kept when there is no
    // page either, to hold the focus: it says why then.
    const bool offered = !b.sha256.empty() || this->page().empty();
    ui::set_visible(install.getView(), offered);

    // The notes: a release's in release-please's form (its sections, their
    // icons) when they are, else any notes and a pull request's description
    // as GitHub's Markdown.
    std::vector<changelog::Line> lines;
    if (b.kind == dev_builds::Kind::Release) lines = changelog::release_notes(b.notes, b.version);
    const std::vector<markdown::Block> blocks = lines.empty() ? markdown::parse(b.notes) : std::vector<markdown::Block>{};
    notes_header->setTitle(b.kind == dev_builds::Kind::Release ? brls::getStr("playshelf/dev_build/notes_release", b.version)
                                                                : "playshelf/dev_build/notes_pr"_i18n);
    ui::set_visible(notes_header.getView(), !lines.empty() || !blocks.empty());
    if (!lines.empty()) release_notes::fill(notes, lines);
    else markdown_view::fill(notes, blocks);
    brls::Application::giveFocus(offered ? install.getView() : github.getView());
}
