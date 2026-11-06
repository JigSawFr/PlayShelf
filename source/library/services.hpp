// services — what a Lantern server holds for a game besides its packages, as
// the screens read it: cheat sets, save backups, game requests; and the cheat
// files on the SD card (Atmosphère's contents/<title id>/cheats/<build id>.txt).
// Filled by lantern_app/services from the SDK's cheats, saves and requests.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace library
{

constexpr size_t MAX_CHEAT_BYTES = 1u << 20;      // a cheat file, 1 MB at most
constexpr size_t MAX_NOTE = 120;                  // a save backup's note (the protocol's limit)

struct Cheat
{
    uint64_t    title_id = 0;
    std::string build_id;   // upper-case hex, 1 to 64 digits
    std::string title_name, note;
    std::string id;         // the server's, to fetch the file
    std::string sha256;     // of the file, 64 hex digits
    uint64_t    size = 0;
};

// The build id as Atmosphère names its file: upper-case hex, 1 to 64 digits;
// "" when it is not one.
std::string normal_build_id(const std::string& build_id);
// The cheat file of a build on the SD card; "" for a bad id.
std::string cheat_path(const std::string& sd_root, uint64_t title_id, const std::string& build_id);
// The builds the SD card has cheats for, for a game, sorted.
std::vector<std::string> installed_cheats(const std::string& sd_root, uint64_t title_id);

struct SaveBackup
{
    uint64_t    title_id = 0;
    std::string save_id;
    std::string title_name, note, created_at;
    int64_t     created = 0;  // POSIX time, 0 when unknown
    uint64_t    size = 0;
    std::string sha256;       // of the archive, 64 hex digits
    bool        latest = false;   // the newest of its game's
};

struct TitleRequest
{
    std::string id;
    uint64_t    title_id = 0;
    std::string title_name, status;   // "open", "closed" (the game is on the server now), "denied"…
    int64_t     created = 0;          // POSIX time, 0 when unknown
};

}   // namespace library
