// services — what an AeroFoil server offers beyond its catalogue, reached
// with the address, credentials and pin its catalogue was read with
// (Link): its cheats (Atmosphère's text files, by game and build), a user's
// save backups (zip archives, several versions of a game's save, each with
// a note) and the games asked of it (its requests). Plain C++ over a
// Transport (client.hpp), as the catalogue.
//
// What AeroFoil serves to a client's credentials (HTTP Basic): a cheat's
// file, the save backups (an account with the Backup permission). What it
// serves only to a signed-in browser: its list of cheats, the requests. For
// those, a Session: the server's web sign-in (POST /login, the same
// username and password), its cookie sent with them, signed in again once
// when the server asks for it.
//
// The answers are read as the catalogue's are: a depth limit, bounded
// strings with no control character, at most MAX_SERVICE_ITEMS entries,
// addresses made absolute and https:// only.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "library/client.hpp"

namespace library
{

constexpr size_t MAX_SERVICE_ITEMS = 20000;
constexpr size_t MAX_CHEAT_BYTES = 1u << 20;      // a cheat file, 1 MB at most
constexpr size_t MAX_NOTE = 120;                  // a save backup's note, as AeroFoil keeps it

// Where the catalogue came from, and how it was asked for.
struct Link
{
    std::string              endpoint;   // "https://host[:port]", no slash at its end
    std::vector<std::string> headers;    // the credentials (HTTP Basic), when there are some
    std::string              pin;        // a LAN server's key
    std::string              username, password;
};
Link link_of(const Server& server, const Fetch& fetch, const std::string& password);

struct Session
{
    std::string cookie;   // "name=value; …" once signed in
};

// The server's web sign-in. False, with why, when it refuses (the username
// or password, a lock after too many tries) or has none.
bool sign_in(const Link& link, Transport& t, Session* session, std::string* why);

// ---- Cheats

struct Cheat
{
    uint64_t    title_id = 0;
    std::string build_id;   // upper-case hex, 1 to 64 digits
    std::string title_name, note;
    std::string url;        // absolute, https://
    uint64_t    size = 0;
};
// AeroFoil's {"success":true,"cheats":[…]}; its error said.
std::optional<std::vector<Cheat>> parse_cheats(const std::string& body, const std::string& base_url,
                                               std::string* why = nullptr);
bool list_cheats(const Link& link, Transport& t, Session& session, std::vector<Cheat>* out, std::string* why);
// A cheat's text (MAX_CHEAT_BYTES at most).
bool fetch_cheat(const Link& link, Transport& t, const Cheat& cheat, std::string* text, std::string* why);

// A build id as Atmosphère names a cheat file: upper-case hex, 1 to 64
// digits; "" when it is not one.
std::string normal_build_id(const std::string& build_id);
// "<sd>/atmosphere/contents/<TID>/cheats/<BID>.txt"; "" for a bad build id.
std::string cheat_path(const std::string& sd_root, uint64_t title_id, const std::string& build_id);
// The builds a game has a cheat file for on the SD card.
std::vector<std::string> installed_cheats(const std::string& sd_root, uint64_t title_id);

// ---- Save backups

struct SaveBackup
{
    uint64_t    title_id = 0;
    std::string save_id;      // "" for the server's single older backup of a game
    std::string title_name, note, created_at;
    int64_t     created = 0;  // POSIX time, 0 when unknown
    uint64_t    size = 0;
    bool        latest = false;        // the newest of its game's
    bool        server_copy = false;   // AeroFoil's "latest directory": what it made of the newest, not a backup of its own
    std::string download_url;          // absolute, https://
    std::string delete_url;            // absolute, https://; "" when it cannot be deleted
};
// {"saves":[…]}: the backups, newest first within a game.
std::optional<std::vector<SaveBackup>> parse_saves(const std::string& body, const std::string& base_url,
                                                   std::string* why = nullptr);
bool list_saves(const Link& link, Transport& t, std::vector<SaveBackup>* out, std::string* why);
// The zip archive `zip_path` sent as a new backup of `title_id`'s save,
// with `note` (bounded to MAX_NOTE, control characters made spaces).
bool upload_save(const Link& link, Transport& t, uint64_t title_id, const std::string& zip_path,
                 const std::string& note, const std::function<void(uint64_t, uint64_t)>& progress,
                 const std::atomic<bool>* cancel, std::string* why);
bool download_save(const Link& link, Transport& t, const SaveBackup& backup, const std::string& zip_path,
                   const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel,
                   std::string* why);
bool delete_save(const Link& link, Transport& t, const SaveBackup& backup, std::string* why);

// ---- Requests

struct TitleRequest
{
    int64_t     id = 0;
    uint64_t    title_id = 0;
    std::string title_name, status;   // "open", "closed" (the game is on the server now), "denied"…
    int64_t     created = 0;          // POSIX time, 0 when unknown
};
std::optional<std::vector<TitleRequest>> parse_requests(const std::string& body, std::string* why = nullptr);
bool list_requests(const Link& link, Transport& t, Session& session, std::vector<TitleRequest>* out, std::string* why);
// Asks the server for `title_id` (a base game). *message: what the server
// answered (a request already open counts as done).
bool request_title(const Link& link, Transport& t, Session& session, uint64_t title_id, const std::string& name,
                   std::string* message, std::string* why);

}   // namespace library
