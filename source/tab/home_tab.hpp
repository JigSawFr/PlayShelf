// HomeTab — where the console and its servers stand, at a glance, so the
// Library and the Catalogue keep to their games (what it shows is made by
// action/insights). A greeting and a line (the console's games, the
// servers' games), and a search of every server (activity/search_activity);
// tiles counting the console's games, the updates and add-ons the servers
// have that it has not (A on the updates: all of them at once, or the
// Library with only those games), the downloads under way and the SD card's
// room (activity/storage_activity); then rows: the console's games played
// lately, the games with something to get (the most played first) as wide
// banners, games for the console's player (library/recommend: like the games
// played most, in their series, by their studios, in their genres; each
// with its reason) and the next ones in the console's series, the wishlist,
// the games new on the servers, the servers' best known and most read lately
// games (action/popular_flow), the month's releases (else the newest) and
// the games coming out soon. A on a game:
// its page (action/game_page). With no server and no game, a welcome to add
// a server or find one.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "action/insights.hpp"
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
    using Pick = insights::Pick;
    // A cover's words under its name, and their colour.
    struct Detail
    {
        std::string text;
        const char* ink = "cadran/text3";
    };

    // Everything built again when what it shows changed (`force`: anyway).
    void build(bool force);
    void add_welcome();
    void add_hero(const insights::Snapshot& s);
    void add_tiles(const insights::Snapshot& s);
    void add_banners(const std::string& title, const std::vector<Pick>& picks, bool to_get);
    void add_covers(const std::string& title, const std::string& line, const std::vector<Pick>& picks,
                    const std::function<Detail(const Pick&)>& detail);
    void add_recent(const std::vector<insights::Played>& recent);
    void add_wished(const std::vector<insights::Wished>& wished);
    void add_coming(const std::vector<insights::Coming>& coming);
    brls::Box* add_row(const std::string& title, const std::string& line, float height);
    // A cover for a game known by its title id alone (no catalogue has it).
    GameTile* lone_tile(uint64_t id, const std::string& name, const Detail& detail);
    // A cover's icon: its catalogue's, else the title databases' or Nlib
    // API's (`tiles`' last; a picture for an older build is dropped).
    void show_icon(const Pick& pick, uint64_t id);

    std::string              built_key;
    std::vector<BannerCard*> cards;
    std::vector<GameTile*>   tiles;
    int                      generation = 0;   // the rows' build: a picture for an older one is freed
    int                      insights_listener = 0;

    BRLS_BIND(brls::Box, content, "ho_content");
};
