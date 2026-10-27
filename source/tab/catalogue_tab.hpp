// CatalogueTab — a library server's catalogue (action/library_flow). With no
// server yet: an empty state, to add one or find one on this network. Else
// the server shown (A: another one, change it, add one, find one, read it
// again), the search (Ⓨ too), the order (name, newest, largest,
// publisher, or shelves: a row of covers a genre, "See all" to filter by it) and the
// section when the server has several; a line saying where the catalogue
// stands (being read, why it could not be, how many games, the server's
// message); the newest games as wide banners (featured, from what Nlib API
// knows) while nothing narrows the catalogue; then the games as covers, a page at a time ("Show more"). A on
// a game: its page (activity/game_activity), to install the base game, an update, an add-on, or
// everything the console does not have of it at once (the base game, its
// newest update, its add-ons). A cover says when the console has the game
// (in amber when the server has a newer update), a line of the page when
// it has that item. The covers' icons come from the server, kept in the
// cache (action/library_icons).
// A game's items go to the downloads (action/download_flow, the Downloads
// tab).
// Ⓧ reads the catalogue again. What is chosen stays for this run.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>
#include <vector>

#include <borealis/cadran/ui/cadran_list.hpp>

#include "action/library_flow.hpp"
#include "library/servers.hpp"
#include "tab/tab_base.hpp"

class BannerCard;
class GameTile;

class CatalogueTab : public TabBase
{
  public:
    CatalogueTab();
    ~CatalogueTab() override;
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    // The server shown: the one chosen in this run, else the first.
    const library::Server* server() const;
    void load(bool again);
    void show_status();
    // The grid built again when what it shows changed (`force`: anyway).
    void build_grid(bool force);
    void add_page();
    // The games by genre: a shelf each, its covers scrolled sideways.
    void add_shelves();
    // The newest games Nlib API knows, as wide banners above the covers,
    // while nothing narrows the catalogue.
    void add_featured();
    // A game's cover, kept in tiles (its icon on its way).
    GameTile* make_tile(size_t title);
    void open_title(size_t title);
    void server_sheet();
    void pick_sort();
    void pick_filter();
    // The games the search, section, filter and order keep, as indexes in
    // shown_data->titles.
    std::vector<size_t> select() const;
    void ask_facts();
    void facts_came();
    void pick_section();
    void search();
    void after_saved(const std::string& saved_id);

    cadran::List list;
    std::vector<library::Server> servers;
    std::shared_ptr<const library_flow::Loaded> shown_data;
    bool                                        waiting = false;   // a fetch asked, its answer not come yet
    std::string why_failed;
    std::vector<size_t> selection;   // the games shown, as indexes in shown_data->titles
    size_t      built = 0;            // how many of them have a cover
    brls::Box*  grid_row = nullptr;
    std::vector<GameTile*> tiles;     // the covers built
    std::vector<size_t> tile_titles;  // theirs, as indexes in shown_data->titles
    int         generation = 0;       // the covers' build: an icon arriving for an older one is freed
    std::vector<BannerCard*> cards;   // the featured banners built
    size_t      featured_count = 0;   // how many games the featured row had to show
    int         featured_generation = 0;   // its build: a banner arriving for an older one is freed
    std::string password;             // the server's, for its icons
    // What the grid was built for.
    const void* built_data = nullptr;
    std::string built_key;
    std::shared_ptr<const library_flow::Installed> built_installed = std::make_shared<library_flow::Installed>();

    BRLS_BIND(brls::Box,        empty_box, "lb_empty");
    int downloads_listener = 0;
    int nlib_listener = 0;
    BRLS_BIND(brls::DetailCell, server_cell, "lb_server");
    BRLS_BIND(brls::DetailCell, search_cell, "lb_search");
    BRLS_BIND(brls::DetailCell, sort_cell,   "lb_sort");
    BRLS_BIND(brls::DetailCell, filter_cell, "lb_filter");
    BRLS_BIND(brls::DetailCell, section_cell, "lb_section");
    BRLS_BIND(brls::Box,        featured,    "lb_featured");
    BRLS_BIND(brls::Label,      status,      "lb_status");
    BRLS_BIND(brls::Box,        grid,        "lb_grid");
    BRLS_BIND(brls::Button,     more,        "lb_more");
};
