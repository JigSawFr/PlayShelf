// recovery — what to do with a download that failed or stands still: why
// it failed (the cause read from its error), whether PlayShelf tries again
// by itself (a network cut, a busy server: 10 s, 30 s then 2 min later,
// from where the journal stands), and what the user can do about it
// otherwise (try the server's other address, change the server's profile,
// find the package again in its catalogue, install it anyway). Also the
// jobs that can no longer be
// done as they stand: their server removed, their package installed by
// now; and a running one no data has reached for a while.
//
// Plain C++ for the host tests (tests/download_recovery).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "download/queue.hpp"
#include "library/catalog.hpp"

namespace download
{

enum class Cause : uint8_t
{
    Network,       // the connection cut, timed out, the host not found
    Server,        // HTTP 5xx, 429: the server busy or broken for now
    Auth,          // HTTP 401, 403: the credentials refused
    Gone,          // HTTP 404, 410: the package no longer at that address
    Certificate,   // the server's key not the one trusted, a certificate refused
    NoSpace,       // not enough free space on the SD card
    Corrupt,       // a content damaged (its hash, its size): the server's file, likely
    Unsigned,      // a content not signed by Nintendo
    ServerGone,    // the job's server removed from PlayShelf (SERVER_GONE)
    Other,
};

// The cause of a failed job: its error, and whether it stopped on an
// unsigned content.
Cause cause_of(const std::string& why, bool unsigned_content = false);
const char* cause_name(Cause c);   // "network", "server", …: the strings' keys

// The error of a job whose server is no longer in servers.json.
extern const char* const SERVER_GONE;

// Seconds before PlayShelf tries again by itself, after `attempts` tries of
// its own already: nothing when it does not (the cause needs the user, or
// the tries are spent).
std::optional<int> retry_delay(Cause c, int attempts);
constexpr int RETRIES = 3;

// What the user can do, besides removing the job.
enum class Remedy : uint8_t
{
    Retry,          // as it stands, from its journal
    OtherAddress,   // the server's other address (LAN ↔ WAN)
    EditServer,     // the server's profile: its credentials, its key
    FindAgain,      // the package found again in its server's catalogue
    InstallAnyway,  // the unsigned content taken
};

struct Situation
{
    bool server_known = false;    // the job's server is in servers.json
    bool other_address = false;   // that server has another address than the job's
};
std::vector<Remedy> remedies(Cause c, const Situation& s);

// `url` moved from the server address `from` to `to` ("https://host:port"
// with or without a path): the same path under `to`; "" when `url` is not
// under `from`.
std::string rebase(const std::string& url, const std::string& from, const std::string& to);

// The console has the job's package already: its title at that version or
// newer (a base game: the title at all). `installed`: title id → version.
bool installed_already(const Job& job, const std::map<uint64_t, uint32_t>& installed);

// The item of a catalogue that is the job's package: the same title id at
// the same version (the first of that title id when the job has no
// version); -1 for none.
long find_again(const Job& job, const std::vector<library::Item>& items);

// No byte for this long, a running job is shown as standing still (the
// transfer itself gives up after 30 s and the job is tried again).
constexpr double QUIET_SHOWN = 10.0;

}   // namespace download
