// LibraryTab — the games on the console (action/installed_flow), by name, as
// covers or as a list (Ⓨ, config library_view): each one's icon and name from the console (else its catalogue's,
// else its title id), and under it what stands out: an update or add-ons a
// catalogue read this run has and the console has not (in amber), else the
// version it runs. A line on top says how many games, and how many have
// something to get (A reads them again). A on a game: its page
// (activity/game_activity), with what the catalogue that has it offers
// (library_flow::game_lines: install its update, its add-ons, everything
// missing at once), else what the console has of it; and to delete it
// (installed_flow::remove, asked first). The catalogues are the
// Catalogue tab's (action/library_flow): the shown server's is read when
// none is yet. Read again when the tab shows and when a download is
// installed; Ⓧ reads it again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "action/installed_flow.hpp"
#include "action/library_flow.hpp"
#include "tab/tab_base.hpp"

class GameTile;

class LibraryTab : public TabBase
{
  public:
    LibraryTab();
    ~LibraryTab() override;
    static brls::View* create();

  protected:
    void refresh() override;

  private:
    // A game of the console and where a catalogue has it.
    struct Entry
    {
        const installed_flow::Game*           game = nullptr;
        std::shared_ptr<const library_flow::Loaded> data;   // nullptr: in no catalogue read
        long                                  title = -1;    // in data->titles
        std::string                           name;
    };

    void show_games(std::shared_ptr<const std::vector<installed_flow::Game>> games);
    void add_page();
    void pick_filter();
    void open(size_t i);
    void set_hints();

    std::shared_ptr<const std::vector<installed_flow::Game>> shown_games;
    std::vector<Entry>     entries;   // by name
    std::vector<std::function<void(int)>> icons;   // puts an icon on each entry built, in order
    brls::Box*             grid_row = nullptr;   // covers: the row being filled
    brls::Box*             rows = nullptr;       // list: the card of rows (in list_box, made once)
    size_t                 built = 0;
    int                    generation = 0;
    int                    listener = 0;
    int                    titledb_listener = 0;
    int                    nlib_listener = 0;
    int                    titledb_opened = 0;   // titledb_flow::Status::opened when its names were read
    size_t                 done_jobs = 0;   // the downloads installed when last read: one more, read again

    BRLS_BIND(brls::Box,    empty_box, "li_empty");
    BRLS_BIND(brls::DetailCell, summary, "li_summary");
    BRLS_BIND(brls::DetailCell, filter_cell, "li_filter");
    BRLS_BIND(brls::Box,    grid,      "li_grid");
    BRLS_BIND(brls::Box,    list_box,  "li_list");
    BRLS_BIND(brls::Button, more,      "li_more");
};
