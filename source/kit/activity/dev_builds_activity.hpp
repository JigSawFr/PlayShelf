// DevBuildsActivity — Help & diagnostics › Developer › Install another build
// (action/dev_build_flow.hpp): a tab each for the releases (pre-releases
// included, tagged), main's last commits and the open pull requests (a long
// list of one kind never hides the next), the builds of the chosen one
// below (view/build_cell.hpp: its subject over its tags, how old it is),
// and the count of newer ones in each tab. Ⓛ / Ⓡ move from one tab to the
// next (their icons on either side of the tabs, not in the footer: no room
// there on the console); the tab opened first is the installed build's,
// then the last one chosen in this run. A on the pull requests' tab, when
// it is shown, sorts them the other way (the newest build first, or by
// number; A's hint says which it goes to, config.json remembers it). Ⓨ finds the build
// of any pull request or commit (signed in). A on a build shows it
// (activity/dev_build_activity.hpp); below the list, signing in to GitHub
// (signed out, only the releases are listed), Go back to the previous build
// (the one installed before, kept: action/rollback_flow.hpp), and how old
// the list is; Ⓧ fetches it again, and so does the screen itself every few
// minutes while a build's checks are still running.
// While the list is fetched, or a build looked for, a bar at the
// header's right shows how far it is.
// A list fetched again while this screen is open replaces its content in
// place: the cells are reused, never freed (one of them may have the focus).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

#include <aurore/cadran/view/loading_bar.hpp>

#include "util/dev_builds.hpp"

class BuildCell;

class DevBuildsActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/dev_builds.xml");

    // Shows `cache`: in this screen when it is open, else in a new one.
    static void show(const dev_builds::Cache& cache);
    // The bar, at `done` (0..1), over `text`, while the screen is open;
    // `steady`: it fills slowly by itself (the length is not known).
    static void loading(const std::string& text, float done, bool steady = false);
    // The bar goes.
    static void loaded();
    // The bar goes, `text` (why nothing could be listed) in its place.
    static void failed(const std::string& text);

    explicit DevBuildsActivity(const dev_builds::Cache& cache);
    ~DevBuildsActivity() override;

    void onContentAvailable() override;
    void onResume() override;

  private:
    void update(const dev_builds::Cache& cache);
    // The tabs and the chosen one's builds from this->cache, in the order chosen.
    void fill();
    // The builds of `kind`, in the order chosen.
    std::vector<dev_builds::Build> builds(dev_builds::Kind kind) const;
    // Shows `kind`'s builds, the focus on the first one when `focus_list`.
    void show_tab(dev_builds::Kind kind, bool focus_list);
    // The next tab with builds, `step` 1 or -1 (Ⓡ, Ⓛ), the focus going along.
    void next_tab(int step);
    // The pull requests' other order (A on their tab when it is shown).
    void sort();

    // How often the list is fetched again while a build's checks run.
    static constexpr aurore::Time REFRESH_MS = 3 * 60 * 1000;

    dev_builds::Cache    cache;
    dev_builds::Kind     tab = dev_builds::Kind::Release;
    // The cells of db_list, kept here: the list's children are not only them
    // (the screen rising into place puts a mover first among them, ui/motion.cpp).
    std::vector<BuildCell*> cells;
    aurore::RepeatingTimer refresh_timer;

    AURORE_BIND(aurore::Label,      note,           "db_note");
    AURORE_BIND(aurore::Box,        loading_box,    "db_loading");
    AURORE_BIND(LoadingBar,       loading_bar,    "db_loading_bar");
    AURORE_BIND(aurore::Label,      loading_text,   "db_loading_text");
    AURORE_BIND(aurore::Box,        tabs,           "db_tabs");
    AURORE_BIND(aurore::Label,      tab_l,          "db_tab_l");
    AURORE_BIND(aurore::Label,      tab_r,          "db_tab_r");
    AURORE_BIND(aurore::Button,     tab_release,    "db_tab_release");
    AURORE_BIND(aurore::Button,     tab_main,       "db_tab_main");
    AURORE_BIND(aurore::Button,     tab_pulls,      "db_tab_pulls");
    AURORE_BIND(aurore::Box,        list,           "db_list");
    AURORE_BIND(aurore::DetailCell, sign_in,        "db_sign_in");
    AURORE_BIND(aurore::DetailCell, back,           "db_back");
    AURORE_BIND(aurore::Label,      checked,        "db_checked");
};
