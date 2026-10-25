// servers — the library servers the user set up, in
// sd:/config/playshelf/servers.json (written whole, atomically): apart from
// config.json, which "Send a report online" sends. A password is kept
// sealed for this console (util/secret_box), never as it is; the LAN
// server's pin once trusted.
//
// to_json / from_json are plain C++ for the host tests; a file that cannot
// be read keeps nothing of it (the next save writes what the app has).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

#include "library/client.hpp"

namespace library
{

constexpr size_t MAX_SERVERS = 16;

std::string         servers_to_json(const std::vector<Server>& servers);
std::vector<Server> servers_from_json(const std::string& text);

std::string         servers_file();
std::vector<Server> load_servers();
bool                save_servers(const std::vector<Server>& servers, std::string* why);

// A new server's id: 16 random hex digits.
std::string new_server_id();
// The password, opened; "" when there is none or it is another console's.
std::string password_of(const Server& s);
// Sealed into s.password_box (an empty one clears it); false when the
// console's key cannot be had.
bool set_password(Server& s, const std::string& password);

// The key trusted for its own certificate, for a person to compare
// ("AB:CD:…", pin_short()); "" when none is.
std::string pin_shown(const Server& s);

// The server of this id in `servers`, nullptr when there is none.
const Server* find_server(const std::vector<Server>& servers, const std::string& id);

// What is wrong with a server as set up, for the editor: "" when nothing.
// Each address https:// (http:// refused: the plan's rule), one at least.
std::string check_server(const Server& s);

}   // namespace library
