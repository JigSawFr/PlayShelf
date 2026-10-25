// BuildCell — a build in Install another build's list, on two lines: its
// title (a commit's or a pull request's subject, without its Conventional
// Commits prefix; "Release 1.2.0"), then its tags; at the right, how old it
// is over its commit. The tags (build_tags, also on the build's page): a
// pull request's number, the kind of change and its scope ("Feature",
// "ui"), breaking, the pull request a commit came from, how its checks
// ended, pre-release, installed (a check) or new (a sparkle). Focusable like a list cell; reused
// when the list changes (set()).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

#include "util/dev_builds.hpp"

struct BuildTag
{
    std::string text;
    NVGcolor    color;
    const char* icon = nullptr;   // a glyph before the text (cadran_icon), or in its place
};

// The tags of `b`: `newer` when it came after the running build.
std::vector<BuildTag> build_tags(const dev_builds::Build& b, bool newer);
// Its title as the list shows it.
std::string build_title(const dev_builds::Build& b);
// A tag as a view: its icon and text in its colour, on a tint of it.
brls::Box* tag_view(const BuildTag& tag, float font_size = 18);

class BuildCell : public brls::RecyclerCell
{
  public:
    BuildCell();
    void set(const dev_builds::Build& b, bool newer);

  private:
    brls::Label* title;
    brls::Box*   tags;
    brls::Label* age;
    brls::Label* commit;
};
