// servers — the Lantern servers the user set up, in
// sd:/config/playshelf/servers.json (written whole, atomically): apart from
// config.json, which "Send a report online" sends. A server is its name,
// its addresses and its `uid`, learned when it paired; the token, the device
// key and the pinned server key are in lantern.json (lantern/credential_store),
// by that uid.
//
// to_json / from_json are plain C++ for the host tests; a file that cannot
// be read keeps nothing of it (the next save writes what the app has).
// A file of a newer schema is read as none and never written over; the
// keys a server has that this build does not know are written back.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

#include "library/server.hpp"

namespace library
{

struct Server
{
    std::string id;     // 16 hex digits, made once
    std::string name;
    std::string lan_url, wan_url;   // https:// each, one at least
    Connection  connection = Connection::Auto;
    std::string uid;    // the server's own (its handshake); "" until it paired
    std::string kept;   // keys this build does not know, as JSON, written back
};

constexpr size_t MAX_SERVERS = 16;
// servers.json's format: one more when a key changes meaning.
constexpr int SERVERS_SCHEMA = 2;
// Schema 1's, read once to carry the servers over to Lantern (their
// credentials for the other protocols are not kept). Delete this with
// servers_old() and its use in load_servers() on 2027-03-01, or two releases
// after the one that brings Lantern, whichever is later.
constexpr int OLD_SERVERS_SCHEMA = 1;

// `previous`: the file's text before, whose other keys are written back.
std::string         servers_to_json(const std::vector<Server>& servers, const std::string& previous = "");
std::vector<Server> servers_from_json(const std::string& text);
// Whether servers.json's text is of a newer schema than this build's.
bool                servers_newer(const std::string& text);
// Whether it is schema 1's (to be written again as schema 2).
bool                servers_old(const std::string& text);

std::string         servers_file();
std::vector<Server> load_servers();
bool                save_servers(const std::vector<Server>& servers, std::string* why);

// A new server's id: 16 random hex digits.
std::string new_server_id();

// The server of this id in `servers`, nullptr when there is none.
const Server* find_server(const std::vector<Server>& servers, const std::string& id);

// What is wrong with a server as set up, for the editor: "" when nothing.
// Each address https:// (http:// refused: the plan's rule), one at least.
std::string check_server(const Server& s);

}   // namespace library
