// HomeTab — where the console and its servers stand, at a glance, so the
// Library and the Catalogue keep to their games. A greeting and a line (the
// console's games, the servers' games); tiles counting the console's games,
// the updates and add-ons the servers have that it has not, and the
// downloads under way (A on one: the Library with only those games, the
// Downloads); then rows: the console's games with something to get and the
// servers' newest games as wide banners (Nlib API's), and games like the
// console's (their genres weighed by how many of its games have each) as
// covers. A on a game: its page (action/game_page). The servers'
// catalogues not read yet in this run are read one after the other. With
// no server and no game, a welcome to add a server or find one.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "action/installed_flow.hpp"
#include "action/library_flow.hpp"
#include "tab/tab_base.hpp"

class BannerCard;
class GameTile;

class HomeTab : public TabBase
{
  public:
    HomeTab();
    ~HomeTab() override;
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    // A game of a catalogue.
    struct Pick
    {
        std::shared_ptr<const library_flow::Loaded> data;
        size_t                                      index = 0;
    };

    // The next server whose catalogue is not read yet, read.
    void read_next();
    // Everything built again when what it shows changed (`force`: anyway).
    void build(bool force);
    void add_welcome();
    void add_hero(size_t console_games, size_t server_games);
    void add_tiles(size_t console_games, size_t updates, size_t dlcs, size_t downloads);
    void add_banners(const std::string& title, const std::vector<Pick>& picks, bool to_get);
    void add_covers(const std::string& title, const std::string& line, const std::vector<Pick>& picks);
    brls::Box* add_row(const std::string& title, const std::string& line, float height);

    std::shared_ptr<const std::vector<installed_flow::Game>> games;
    std::set<std::string> tried;   // the servers whose catalogue was asked in this run
    std::string           built_key;
    std::vector<BannerCard*> cards;
    std::vector<GameTile*>   tiles;
    int                      generation = 0;   // the rows' build: a picture for an older one is freed
    int                      downloads_listener = 0;
    int                      nlib_listener = 0;

    BRLS_BIND(brls::Box, content, "ho_content");
};
