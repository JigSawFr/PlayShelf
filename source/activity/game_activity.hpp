// GameActivity — a game's page, from the Catalogue (a server's game) or the
// Library (one on the console): its banner, its icon, name and facts, what
// it is about in a line or two, its lines (what to install of it, the
// Library's delete), its screenshots (A shows one on the whole screen, ◀ ▶
// the others), then its description. The banner, the summary, the
// screenshots, the publisher, the date and the categories come from Nlib
// API (action/nlib_flow) in the language PlayShelf speaks, once it answers;
// without it (config nlib_api off, offline, a game it does not know) the
// page shows what the caller gave. A game known only by its title id takes
// Nlib API's name. A line run closes the page first, as the
// sheet it replaces did.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <borealis/cadran/ui/guarded.hpp>
#include <borealis/cadran/ui/ui.hpp>

#include "action/library_icons.hpp"
#include "titledb/nlib.hpp"

class GameActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/game.xml");

    struct Page
    {
        std::string             name;
        uint64_t                title_id = 0;   // 0: unknown (Nlib API is not asked)
        std::string             facts;          // under its name: title id, size, version installed
        std::string             note;           // a line under them ("" none)
        std::vector<ui::Choice> lines;
        // Asks for its icon: the setter given, called on the UI thread with
        // its pixels (library_icons; nullptr: none). nullptr: no icon to ask for.
        std::function<void(library_icons::Done)> icon;
    };
    static void show(Page page);

    explicit GameActivity(Page shown) : page(std::move(shown)) {}
    ~GameActivity() override;

    void onContentAvailable() override;

  private:
    Page        page;
    ui::Alive   alive = ui::make_alive();
    brls::Image* icon = nullptr;
    brls::Label* title = nullptr;
    brls::Label* meta = nullptr;

    void add_lines();
    void show_details(const titledb::Details& d);
    void add_screens(int count);

    BRLS_BIND(brls::Image,  banner,        "gp_banner");
    BRLS_BIND(brls::Box,    head,          "gp_head");
    BRLS_BIND(brls::Label,  intro,         "gp_intro");
    BRLS_BIND(brls::Box,    lines_box,     "gp_lines");
    BRLS_BIND(brls::Header, screens_header, "gp_screens_header");
    BRLS_BIND(brls::Box,    screens_box,   "gp_screens");
    BRLS_BIND(brls::Header, about_header,  "gp_about_header");
    BRLS_BIND(brls::Label,  description,   "gp_description");
    BRLS_BIND(brls::Label,  source,        "gp_source");
};
