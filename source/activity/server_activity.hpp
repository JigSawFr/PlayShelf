// ServerActivity — Library › a server, added or changed (library/servers):
// its name, its address on this network and over the Internet (https://
// each, one at least), which one to use (automatic: this network's when it
// answers), how it answers (the API, a JSON index, or found out), the
// account it asks for (the password sealed for this console as soon as it
// is saved), the key trusted for its own certificate (A forgets it: the next
// connection asks again). Save writes servers.json and runs `saved` with the
// server's id; Delete, once confirmed, removes it (`saved` with "").
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>

#include "library/client.hpp"

class ServerActivity : public brls::Activity
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
    bool            password_changed = false;
    std::string     password;   // the one typed, until saved
    std::function<void(const std::string&)> saved;

    BRLS_BIND(brls::DetailCell, name,       "sv_name");
    BRLS_BIND(brls::DetailCell, lan,        "sv_lan");
    BRLS_BIND(brls::DetailCell, wan,        "sv_wan");
    BRLS_BIND(brls::DetailCell, connection, "sv_connection");
    BRLS_BIND(brls::DetailCell, format,     "sv_format");
    BRLS_BIND(brls::DetailCell, user,       "sv_user");
    BRLS_BIND(brls::DetailCell, pass,       "sv_password");
    BRLS_BIND(brls::DetailCell, pin,        "sv_pin");
    BRLS_BIND(brls::DetailCell, save_cell,  "sv_save");
    BRLS_BIND(brls::DetailCell, delete_cell, "sv_delete");
};
