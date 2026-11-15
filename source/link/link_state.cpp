// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "link/link_state.hpp"

#include "bl_json.h"

namespace link_state
{

Summary summarize(const std::vector<download::Job>& jobs, uint64_t running_done, uint64_t running_total,
                  bool free_known, uint64_t free_bytes, const std::string& version)
{
    Summary s;
    for (const download::Job& j : jobs) {
        switch (j.state) {
        case download::State::Running: s.running++; s.pending++; break;
        case download::State::Waiting: s.pending++; break;
        case download::State::Paused:  s.paused++; s.pending++; break;
        case download::State::Failed:  s.failed++; break;
        case download::State::Done:    s.last_installed = j.game.empty() ? j.name : j.game; break;
        }
    }
    if (s.running && running_total > 0) {
        const uint64_t done = running_done > running_total ? running_total : running_done;
        s.progress = static_cast<int>(done * 100 / running_total);
    }
    s.free_known = free_known;
    s.free_bytes = free_bytes;
    s.version = version;
    return s;
}

std::string state_json(const Summary& s)
{
    char buf[1024];
    BlJson j;
    bl_json_init(&j, buf, sizeof(buf));
    bl_json_obj(&j);
    bl_json_kint(&j, "schema", STATE_SCHEMA);
    bl_json_kstr(&j, "version", s.version.c_str());
    bl_json_key(&j, "downloads");
    bl_json_obj(&j);
    bl_json_kint(&j, "pending", s.pending);
    bl_json_kint(&j, "running", s.running);
    bl_json_kint(&j, "paused", s.paused);
    bl_json_kint(&j, "failed", s.failed);
    bl_json_kint_or_null(&j, "progress", s.progress >= 0, s.progress);
    bl_json_obj_end(&j);
    bl_json_kstr_or_null(&j, "last_installed", s.last_installed.c_str());
    bl_json_key(&j, "storage");
    bl_json_obj(&j);
    bl_json_kint_or_null(&j, "free", s.free_known, static_cast<int64_t>(s.free_bytes));
    bl_json_obj_end(&j);
    bl_json_obj_end(&j);
    const size_t n = bl_json_end(&j);
    return j.overflow ? std::string() : std::string(buf, n);
}

Order parse_order(std::string_view sub, std::string_view payload)
{
    if (sub == "pause_downloads/set" && payload == "PRESS") return Order::PauseDownloads;
    return Order::None;
}

}   // namespace link_state
