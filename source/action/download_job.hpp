// download_job — what one download does on its worker thread: the package
// read from the SD card (what a fetch ahead left) and from the library
// server where the installer asks, installed to the SD card from the job's
// journal (install::Installer). The queue (action/download_flow) owns the
// state; this tells it how far the install is through `Hooks`, on the UI
// thread, and how it ended through the Outcome.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "download/queue.hpp"
#include "install/installer.hpp"
#include "library/servers.hpp"

namespace download_job
{
// What the UI thread tells the running job.
struct Control
{
    std::atomic<bool> pause{false}, cancel{false};
    std::atomic<bool> stop{false};   // either: the download itself cut at once
    std::atomic<bool> cut{false};    // the pause is the transfer's, not the user's
};

struct Outcome
{
    bool             ok = false, paused = false, cancelled = false, unsigned_content = false, compressed_content = false;
    bool             undone = false;   // failed in the install: what every run did undone, the journal of no use
    std::string      why;
    uint64_t         total = 0;
    std::map<uint64_t, bool> programs;   // the program NCAs written, by base game: true for a rebuilt one
};

// Told on the UI thread: how far the install is (about twice a second), and
// the journal to keep once it changed.
struct Hooks
{
    std::function<void(uint64_t done, uint64_t total)> progress;
    std::function<void(const install::Journal&)>       journal;
};

// The size of `path`, 0 when it is not there.
uint64_t file_size(const std::string& path);

// Runs `job` on `server` from `fetched` (the file a fetch ahead left, may be
// missing or short); `snapshot` is the queue, for the placeholders no job
// owns. On the worker thread.
Outcome run(const download::Job& job, const library::Server& server, const std::vector<download::Job>& snapshot,
            const std::shared_ptr<Control>& ctl, const std::string& fetched, const Hooks& hooks);

}   // namespace download_job
