// DevBuildActivity — one build of Install another build
// (activity/dev_builds_activity.hpp), before it is installed: tags for its
// kind, how its checks ended, installed or newer than the installed one;
// its title; a card of facts (when it was built and how long ago, its size,
// its commit and branch, its checksum, when GitHub deletes it); what
// installing it does; Install (the PIN first when one is set) and See on
// GitHub (its page as a QR code, for a phone); then its notes: a release's
// changelog, a pull request's description, in GitHub's Markdown
// (view/markdown_view.hpp). B goes back to the list.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include "util/dev_builds.hpp"

class DevBuildActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/dev_build.xml");

    // `newer`: it came after the running build (the list knows).
    static void show(const dev_builds::Build& build, bool newer = false);

    DevBuildActivity(const dev_builds::Build& shown, bool after_running) : build(shown), newer(after_running) {}

    void onContentAvailable() override;

  private:
    dev_builds::Build build;
    bool              newer;

    void add_fact(const std::string& name, const std::string& value, NVGcolor color);
    // Its page on GitHub ("" when there is none to show).
    std::string page() const;
    void show_page();

    BRLS_BIND(brls::Box,    tags,         "dbd_tags");
    BRLS_BIND(brls::Label,  headline,     "dbd_headline");
    BRLS_BIND(brls::Box,    facts,        "dbd_facts");
    BRLS_BIND(brls::Label,  warning,      "dbd_warning");
    BRLS_BIND(brls::Button, install,      "dbd_install");
    BRLS_BIND(brls::Button, github,       "dbd_github");
    BRLS_BIND(brls::Header, notes_header, "dbd_notes_header");
    BRLS_BIND(brls::Box,    notes,        "dbd_notes");
};
