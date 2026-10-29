// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/update_activity.hpp"

#include "action/dev_build_flow.hpp"
#include "app.hpp"
#include "core/sysinfo.h"
#include "ui/ui.hpp"
#include "util/changelog.hpp"
#include "view/release_notes.hpp"

using namespace aurore::literals;

namespace
{
// The release's notes as lines: those under its own heading (release-please
// writes "## [1.2.0](…) (2026-10-10)" first), else every line of them.
std::vector<changelog::Line> notes_of(const dev_builds::Build& b, std::string* date)
{
    std::vector<changelog::Line> lines = changelog::release_notes(b.notes, b.version, date);
    if (lines.empty()) lines = changelog::parse(b.notes, 0);
    return lines;
}
}   // namespace

void UpdateActivity::show(const update_flow::Offer& offer)
{
    aurore::Logger::info("update: offering {}", offer.result.latest.version);
    auto* activity = new UpdateActivity();
    activity->offer = offer;
    aurore::Application::pushActivity(activity);
}

void UpdateActivity::onContentAvailable()
{
    headline->setSingleLine(false);
    body->setSingleLine(false);
    notes_none->setSingleLine(false);

    const std::string& version = this->offer.result.latest.version;
    headline->setText(aurore::getStr("playshelf/update/headline", version));

    std::string date;
    const auto lines = this->offer.has_release ? notes_of(this->offer.release, &date) : std::vector<changelog::Line>{};
    if (date.empty() && this->offer.has_release) date = dev_builds::short_date(this->offer.release.date).substr(0, 10);
    std::string text = aurore::getStr("playshelf/update/running", app::version());
    if (!date.empty()) text = aurore::getStr("playshelf/update/released", date) + "\n" + text;
    if (this->offer.result.verdict == update::Verdict::UpdateNoSupport) {
        const std::string fw = ui::hos_text(sysinfo_hos_version());
        text += "\n" + aurore::getStr("playshelf/update/available_no_fw", fw);
    }
    body->setText(text);

    notes_header->setTitle(aurore::getStr("playshelf/about/section_changelog", version));
    release_notes::fill(notes, lines);
    const std::string releases = std::string(app::repo_url()) + "/releases";
    notes_none->setText(aurore::getStr(lines.empty() ? "playshelf/update/notes_none" : "playshelf/about/changelog_note",
                                     releases));

    update->registerClickAction([](aurore::View*) {
        update_flow::update_now();
        return true;
    });
    method->registerClickAction([this](aurore::View*) {
        update_flow::choose_method([this]() { this->refresh(); });
        return true;
    });
    later->registerClickAction([version](aurore::View*) {
        update_flow::postpone(version, []() { aurore::Application::popActivity(); });
        return true;
    });
    this->refresh();
    aurore::Application::giveFocus(update.getView());
}

void UpdateActivity::refresh()
{
    update->setText(update_flow::update_label());
    method->setDetailText(update_flow::method_text());
}
