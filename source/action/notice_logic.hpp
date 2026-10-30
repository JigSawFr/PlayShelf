// notice_logic — what the notification bell holds (action/notices has the
// UI side): one entry per thing worth the user's eye, set and cleared by the
// part of PlayShelf that sees it. Two kinds: "to do" (updates to get, a
// game of the wishlist on a server, the SD card nearly full), counted on
// the bell, which stays until what it says is done (only its source clears
// it); and "for information" (new games on the servers, a download ended,
// a new build installed), which the user can put away: one put away comes
// back only when its text changes, or when its source clears it and sets
// it again. Each kind in the order its entries were first set.
// Plain C++ so the host tests can run it (tests/notice_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace notice_logic
{

enum class Level
{
    Action,   // to do: counted on the bell, cleared only by its source
    Info,     // for information: the user can put it away
};

struct Item
{
    std::string id;
    Level       level = Level::Info;
    std::string title;
    std::string detail;
    uint64_t    order = 0;   // when it was first set: entries keep their place
};

class Store
{
  public:
    // Adds the entry, or replaces the one with the same id. True when what
    // the bell shows changed. One for information whose read_key() is in
    // `read` (put away at an earlier start) stays away.
    bool set(const std::string& id, Level level, const std::string& title, const std::string& detail);
    // Removes it, and forgets that it was put away. True when it was shown.
    bool clear(const std::string& id);
    // Puts away an Info entry. False for an Action entry or an unknown id.
    bool dismiss(const std::string& id);

    bool has(const std::string& id) const;   // set and shown

    // What the bell lists: the Action entries, then the Info ones, each kind
    // in the order they were first set.
    std::vector<Item> visible() const;
    int action_count() const;   // the bell's number
    int info_count() const;     // the Info entries shown: the bell's dot

    // The read_key()s of the Info entries put away, for the next start; and
    // those read back then.
    std::vector<std::string> read_keys() const;
    void                     set_read(const std::vector<std::string>& keys);

  private:
    struct Slot
    {
        Item item;
        bool dismissed = false;
    };
    std::vector<Slot>        slots;
    std::vector<std::string> read;   // put away at an earlier start
    uint64_t                 next_order = 1;

    Slot*       find(const std::string& id);
    const Slot* find(const std::string& id) const;
};

// An entry for information put away: its id and a hash of its text
// ("new_games:1a2b3c4d"), kept on the SD card, so it stays put away at the
// next start until its text changes.
std::string read_key(const std::string& id, const std::string& title, const std::string& detail);

// The read keys as the file holds them (a "schema=N" line,
// util/text_schema.hpp, then one per line) and read back (the lines that
// look like one, MAX_READ at most; none from a file of a newer schema,
// which keys_newer() tells: never written over).
constexpr size_t         MAX_READ = 200;
constexpr int            KEYS_SCHEMA = 1;
std::string              keys_text(const std::vector<std::string>& keys);
std::vector<std::string> keys_of(const std::string& text);
bool                     keys_newer(const std::string& text);

}   // namespace notice_logic
