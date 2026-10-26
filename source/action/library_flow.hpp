// library_flow — what the Catalogue tab does with its servers, on the
// network in the background: a server's catalogue fetched (library/client)
// and gathered by game (library/titles), kept for this run so the tab shows
// it again at once; a LAN server's own certificate shown to be trusted
// (its key's fingerprint, to compare with the server's: pin on first use)
// and the fetch made again once it is; the servers of this network found
// (library/http_transport's discovery) to add one; and a game's items
// added to the downloads (action/download_flow), which send the
// credentials and the pin only to the server they are for.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "library/client.hpp"
#include "library/titles.hpp"
#include "ui/ui.hpp"

namespace library_flow
{

struct Loaded
{
    library::Server              server;   // as it was fetched with
    library::Fetch               fetch;
    std::vector<library::Title>  titles;
};

// The catalogue last fetched for `server_id` in this run; nullptr when none.
std::shared_ptr<const Loaded> loaded(const std::string& server_id);
// Forgets it (the server changed or went).
void forget(const std::string& server_id);

// Fetches `server`'s catalogue; `done` on the UI thread with it, or nullptr
// and why not. A LAN certificate not trusted yet asks first: trusted, the
// pin is saved with the server and the fetch made again; refused, `done`
// gets nullptr and "" (nothing to show). Only one fetch at a time: another
// one asked meanwhile waits for it, its `done` given that fetch's result
// (a tab made again while its catalogue is read: the one shown is told).
void load(const library::Server& server, const std::function<void(std::shared_ptr<const Loaded>, std::string)>& done);
bool loading();

// The servers that answer on this network, in a list to add one from
// (activity/server_activity, its address filled); `saved` as the editor's.
void discover(const std::function<void(const std::string& id)>& saved);
// The editor, for a new server or `server`.
void add(const std::function<void(const std::string& id)>& saved);
void edit(const library::Server& server, const std::function<void(const std::string& id)>& saved);

// What the console has installed (install::installed_versions), read with
// each catalogue and again after an install; empty until then.
using Installed = std::map<uint64_t, uint32_t>;
std::shared_ptr<const Installed> installed();

// What the console has installed, read again (an install ended); `after`
// then, on the UI thread.
void reread_installed(const std::function<void()>& after);

// These items of `l`'s catalogue (indexes in fetch.catalog.items) added to
// the downloads (action/download_flow); how many were (one already
// waiting or running is not added twice).
size_t install(const Loaded& l, const std::vector<size_t>& items);

// A game of a catalogue (`index` in data->titles) as a sheet: everything
// the console does not have of it at once (the base game, its newest update
// when newer than the one installed, its add-ons), then each item, its size
// or that the console has it; A on one adds it to the downloads. `more`:
// lines of the caller's after them (the Library's: delete the game).
void game_sheet(const std::shared_ptr<const Loaded>& data, size_t index, const std::vector<ui::Choice>& more = {});

// "734 MB", "4,2 GB" (the language's decimal mark): a package's size.
std::string size_text(uint64_t bytes);

}   // namespace library_flow
