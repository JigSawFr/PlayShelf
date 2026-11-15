// link_state — what PlayShelf says of itself to Home Assistant, through the
// broker (boreale/<id>/playshelf/…): the `state` document (schema 1) and the
// orders it takes (`<entity>/set`). Pure: the downloads' queue and the
// console's figures in, JSON out, and a topic and a payload in, an order out;
// the part that talks to the agent is action/link_flow.
//
// The document is retained and small: the downloads' counts, the running
// one's progress (whole percents, so it does not change every second), the
// last game installed, the SD card's room, the version. A key added keeps
// the schema; a key that changes meaning raises it (AGENTS.md of the
// workbench, "Formats versionnés").
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "download/queue.hpp"

namespace link_state
{

constexpr int STATE_SCHEMA = 1;
constexpr const char* MODULE = "playshelf";

struct Summary
{
    uint32_t    pending = 0;     // not finished: waiting, running or paused
    uint32_t    running = 0;
    uint32_t    paused = 0;
    uint32_t    failed = 0;
    int         progress = -1;   // the running job's, 0..100; -1 when none runs or its size is unknown
    std::string last_installed;  // the last finished game's name in the queue, "" when none
    bool        free_known = false;
    uint64_t    free_bytes = 0;  // the SD card's
    std::string version;
};

// `running_done` / `running_total`: the running job's bytes (download_flow::live()).
Summary summarize(const std::vector<download::Job>& jobs, uint64_t running_done, uint64_t running_total,
                  bool free_known, uint64_t free_bytes, const std::string& version);

// The `state` document, one JSON object. The same summary gives the same text.
std::string state_json(const Summary& s);

enum class Order { None, PauseDownloads };

// The order a message below the module's level is: `sub` is the topic
// after `playshelf/` (e.g. "pause_downloads/set"), `payload` what came. A
// button is pressed with "PRESS"; anything else, another entity or an empty
// payload (a retained order cleared) is no order.
Order parse_order(std::string_view sub, std::string_view payload);

}   // namespace link_state
