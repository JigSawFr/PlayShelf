// seen — when each game of a server was first seen there, for Home's "New
// on your servers": the base games of each catalogue read, kept by server
// on the SD card. A server's first reading dates none of them (they were
// there before PlayShelf knew it); a game seen later is dated with the day
// it first was. A date kept MAX_DAYS, then the game is only known. Also the
// wishlist's file: title ids, one per line.
//
// Plain C++ for the host tests (tests/library_seen).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace library
{

class SeenBook
{
  public:
    static constexpr size_t   MAX_GAMES = 100000;   // a server's, at most
    static constexpr size_t   MAX_SERVERS = 64;
    static constexpr uint32_t MAX_DAYS = 90;

    // `server_id`'s catalogue read on `day` (YYYYMMDD) with these base
    // games. True when the book changed (to be written).
    bool note(const std::string& server_id, const std::vector<uint64_t>& ids, uint32_t day);
    // The day `id` was first seen on `server_id`; 0 when it was there from
    // the first reading, its date is older than MAX_DAYS, or it is unknown.
    uint32_t first_seen(const std::string& server_id, uint64_t id) const;
    // A server gone.
    bool forget(const std::string& server_id);

    std::string to_json() const;
    bool        from_json(const std::string& text);
    // Whether seen.json's text is of a newer schema than this build's: not
    // read, and never written over.
    static bool newer(const std::string& text);

  private:
    struct Server
    {
        std::set<uint64_t>           known;
        std::map<uint64_t, uint32_t> dated;   // the games seen after the first reading, by the day they were
    };
    std::map<std::string, Server> servers;
};

// The days between two YYYYMMDD dates (b - a), by the calendar.
long days_between(uint32_t a, uint32_t b);

// Title ids as a file holds them, one per line in hex, and read back (the
// lines that are one, `max` at most, each once, in order).
std::string           ids_text(const std::vector<uint64_t>& ids);
std::vector<uint64_t> ids_of(const std::string& text, size_t max);

}   // namespace library
