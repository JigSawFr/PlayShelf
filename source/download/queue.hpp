// queue — the downloads PlayShelf installs one after the other, kept in
// sd:/config/playshelf/queue.json (written whole, atomically) so that a
// restart loses none of them: each package's address and where it comes
// from (a library server's id: its credentials and its pin are taken from
// its profile when the install starts, never written here), its state, and
// the install's journal (install::Journal) to take it up again. A job that
// was running when PlayShelf stopped (a crash, the console turned off)
// comes back paused.
//
// Also what the queue screen shows: the speed over the last seconds and the
// time left; and the placeholders no job's journal keeps (an install cut
// short and given up, another installer's), to be deleted.
//
// Plain C++ for the host tests (tests/download_queue).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "install/installer.hpp"

namespace download
{

enum class State : uint8_t { Waiting, Running, Paused, Done, Failed };

struct Job
{
    std::string id;          // 16 hex digits, made once
    std::string name;        // shown: the item's name
    std::string url;         // https://
    std::string server_id;   // the library server's (library/servers), "" for none
    uint64_t    title_id = 0;
    uint64_t    size = 0;    // the package's, as the catalogue gives it (0: unknown)
    State       state = State::Waiting;
    std::string error;       // Failed: why
    bool        unsigned_content = false;   // Failed: on a content not signed by Nintendo
    bool        accept_unsigned = false;    // the user said to install it anyway
    uint64_t    done = 0, total = 0;        // the install's bytes, as last known
    std::optional<install::Journal> journal;
};

constexpr size_t MAX_JOBS = 256;

std::string      queue_to_json(const std::vector<Job>& jobs);
std::vector<Job> queue_from_json(const std::string& text);

std::string      queue_file();
std::vector<Job> load_queue();
bool             save_queue(const std::vector<Job>& jobs, std::string* why);

std::string new_job_id();

// The job to start next: the first waiting one, -1 when none.
long next_job(const std::vector<Job>& jobs);
// The jobs not finished: waiting, running or paused.
size_t pending(const std::vector<Job>& jobs);
// A package already in the queue and not finished: the same address.
bool queued(const std::vector<Job>& jobs, const std::string& url);

// The placeholders of `present` that no job's journal keeps.
std::vector<install::ContentId> orphans(const std::vector<install::ContentId>& present, const std::vector<Job>& jobs);

// The speed of a download: bytes over the last WINDOW seconds.
class Rate
{
  public:
    static constexpr double WINDOW = 5.0;
    // `bytes`: how many so far, at `seconds` (any clock that only goes on).
    void   add(uint64_t bytes, double seconds);
    void   reset() { samples.clear(); }
    // 0 until two samples a second apart at least.
    double bytes_per_second() const;
    // Seconds left for `left` bytes at that speed; nothing when unknown.
    std::optional<double> eta(uint64_t left) const;

  private:
    std::deque<std::pair<double, uint64_t>> samples;
};

}   // namespace download
