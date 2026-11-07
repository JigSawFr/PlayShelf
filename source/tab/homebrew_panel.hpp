// HomebrewPanel — the homebrew a library server offers (SPEC.md § 8,
// action/homebrew_flow) as tiles in a box a tab gives it (the Catalogue's
// "Homebrew" view): the catalogue read for the server shown, its entries as
// tiles like a game's (icon from the server, name, version, or that the
// console has it, in amber with an update), the ones a search keeps, a line
// saying where the reading stands. A on one: its sheet, to install it,
// update it, install it again or remove it (a path that needs consent is
// asked first), and for a port to look for its game data on the SD card.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <aurore.hpp>
#include <aurore/cadran/ui/guarded.hpp>

#include "action/homebrew_flow.hpp"
#include "library/servers.hpp"

class GameTile;
class ToolChip;

// The "Show" pill both tabs carry: the games, or the homebrew, of a server.
namespace homebrew_kind {
// The pill in step with the choice (`homebrew`: the homebrew is shown).
void show(ToolChip* chip, bool homebrew);
// Asks which of the two to show; `then` gets true for the homebrew.
void pick(bool homebrew, std::function<void(bool)> then);
}

class HomebrewPanel
{
  public:
    // The tiles go in `host`, the reading's state in `status`.
    HomebrewPanel(aurore::Box* tiles_box, aurore::Label* status_label);
    ~HomebrewPanel();

    // Shows `server`'s homebrew, read first when it was not (or another
    // server's was): the entries whose name holds `query`, only the ones the
    // console has when `installed_only`. Built again only when it changed.
    // `order`: by name, the ones with an update first, or the largest first.
    enum class Order { Name, Updates, Largest };
    void show(const library::Server& server, const std::string& query, bool installed_only, Order order = Order::Name);
    // Reads the server again.
    void reload();
    // How many entries the console has (what the last show() was given).
    size_t count() const { return this->shown_count; }

  private:
    void load();
    void present(const std::shared_ptr<const homebrew_flow::Loaded>& data, const std::string& why);
    void rebuild();
    GameTile* make_tile(size_t index);
    void open_item(size_t index);
    void install(size_t index);
    void remove(const std::string& id);
    void check_data(size_t index);

    aurore::Box*   host;
    aurore::Label* status;
    ui::Alive      alive = ui::make_alive();

    library::Server server;
    std::string     query, built_key, why_failed;
    bool            installed_only = false, waiting = false;
    Order           order = Order::Name;
    size_t          shown_count = 0;
    std::shared_ptr<const homebrew_flow::Loaded> data;
    std::vector<GameTile*> tiles;   // the tiles shown, in the order of `entries`
    std::vector<size_t>    entries; // theirs, as indexes in data->list.items
    int                    generation = 0;   // an icon on its way finds its tile gone
};
