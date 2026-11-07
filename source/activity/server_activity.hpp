// ServerActivity — Library › a server, added or changed (library/servers):
// its name, its address on this network and over the Internet (https://
// each, one at least), which one to use (automatic: this network's when it
// answers), and whether this console is paired with it (A pairs it again or
// forgets the pairing). Save writes servers.json, then pairs a server that
// is not yet, and runs `saved` with the server's id; Delete, once confirmed,
// removes it and the console's credentials for it (`saved` with "").
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <functional>
#include <string>

#include "library/servers.hpp"

class ServerActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/server.xml");

    // `is_new`: not yet in servers.json (added, or found on this network).
    ServerActivity(library::Server server, bool is_new, std::function<void(const std::string& id)> saved);

    void onContentAvailable() override;

  private:
    void show();
    void save();
    void remove();
    // An address asked with the keyboard: kept when https://, "" clears it.
    void ask_url(std::string* url);

    library::Server server;
    bool            is_new;
    std::function<void(const std::string&)> saved;

    AURORE_BIND(aurore::DetailCell, name,       "sv_name");
    AURORE_BIND(aurore::DetailCell, lan,        "sv_lan");
    AURORE_BIND(aurore::DetailCell, wan,        "sv_wan");
    AURORE_BIND(aurore::DetailCell, connection, "sv_connection");
    AURORE_BIND(aurore::DetailCell, pairing,    "sv_pairing");
    AURORE_BIND(aurore::DetailCell, save_cell,  "sv_save");
    AURORE_BIND(aurore::DetailCell, delete_cell, "sv_delete");
};
