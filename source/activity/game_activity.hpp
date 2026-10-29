// GameActivity — a game's page, from the Catalogue (a server's game) or the
// Library (one on the console). On top, its banner, its icon, name and
// facts, a line of its publisher and year, and chips: its genres, how many
// play it, the language PlayShelf speaks when the game has it, a demo. Then
// tabs (A on one, or L and R): Overview (what it is about, what to install
// of it and the caller's lines, its screenshots: A shows one on the whole
// screen, ◀ ▶ the others; its description, then more games of its
// publisher and of its genre, from the caller), Updates (the versions
// installed and newest, the newest at once, each update), Add-ons (how many
// the console has, the missing ones at once, each add-on) and Details (its
// ids, sizes, players, languages, age rating, the version installed, its
// eShop page as a QR code). Updates and Add-ons show when it has some.
//
// What Nlib API knows of it (action/nlib_flow) fills in once it answers;
// without it (config nlib_api off, offline, a game it does not know) the
// page shows what the caller gave. A game known only by its title id takes
// Nlib API's name. A line run closes the page first.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <borealis/cadran/ui/guarded.hpp>
#include <borealis/cadran/ui/ui.hpp>

#include "action/library_flow.hpp"
#include "action/library_icons.hpp"
#include "titledb/nlib.hpp"

class GameActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/game.xml");

    // Asks for an icon: the setter given, called on the UI thread with its
    // pixels (library_icons; nullptr: none).
    using IconAsk = std::function<void(library_icons::Done)>;

    // Another game the page offers (more of its publisher, of its genre).
    struct Other
    {
        std::string           name;
        IconAsk               icon;
        std::function<void()> open;
    };

    struct Page
    {
        std::string             name;
        uint64_t                title_id = 0;   // 0: unknown (Nlib API is not asked)
        std::string             note;           // a line under its facts ("" none)
        library_flow::GameLines lines;          // its tabs' lines and numbers
        IconAsk                 icon;           // nullptr: no icon to ask for
        // More games of a publisher, of a genre (Nlib API's English word):
        // the caller's, nullptr for none.
        std::function<std::vector<Other>(const std::string& publisher)> by_publisher;
        std::function<std::vector<Other>(const std::string& genre)>     by_genre;
        // The games most like it the servers have (PlayInsights' similar.json).
        std::function<std::vector<Other>()> similar;
    };
    static void show(Page page);

    explicit GameActivity(Page shown) : page(std::move(shown)) {}
    ~GameActivity() override;

    void onContentAvailable() override;

  private:
    Page                     page;
    ui::Alive                alive = ui::make_alive();
    brls::Image*             icon = nullptr;
    brls::Label*             title = nullptr;
    brls::Label*             meta = nullptr;
    brls::Box*               chips = nullptr;
    std::vector<brls::Box*>  tabs;      // the tab bar's tabs
    std::vector<brls::Box*>  panes;     // each tab's content
    size_t                   current = 0;
    brls::Image*             banner = nullptr;   // gp_hero's first child
    std::shared_ptr<const titledb::Details> details;

    void add_head();
    void add_tabs();
    void select(size_t tab, bool focus);
    void add_lines(brls::Box* pane, const std::vector<ui::Choice>& lines);
    void show_details(std::shared_ptr<const titledb::Details> d);
    void show_chips();
    // The head over the banner, on a shade fading into the page.
    void over_banner();
    void fill_details_tab();
    void add_screens(int count);
    void add_others(const std::string& title, std::vector<Other> others);

    BRLS_BIND(brls::Box,   hero,     "gp_hero");
    BRLS_BIND(brls::Box,   head,     "gp_head");
    BRLS_BIND(brls::Box,   tab_bar,  "gp_tabs");
    BRLS_BIND(brls::Box,   overview, "gp_overview");
    BRLS_BIND(brls::Box,   updates,  "gp_updates");
    BRLS_BIND(brls::Box,   dlcs,     "gp_dlcs");
    BRLS_BIND(brls::Box,   facts,    "gp_details");
    BRLS_BIND(brls::Label, intro,    "gp_intro");
    BRLS_BIND(brls::Box,   lines_box, "gp_lines");
    BRLS_BIND(brls::Header, screens_header, "gp_screens_header");
    BRLS_BIND(brls::Box,   screens_box, "gp_screens");
    BRLS_BIND(brls::Header, about_header, "gp_about_header");
    BRLS_BIND(brls::Label, description, "gp_description");
    BRLS_BIND(brls::Box,   others_box, "gp_others");
    BRLS_BIND(brls::Label, source,   "gp_source");
};
