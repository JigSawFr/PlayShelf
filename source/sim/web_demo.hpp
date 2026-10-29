// web_demo — the web simulator's library server (web/README.md): its
// address, which source/sim/web_http.cpp answers from the files
// tools/web_demo.cpp made, packaged with the page at DEMO_DIR. The page adds
// it to servers.json on a first start (web/simulator.js). With it,
// PlayInsights' files of its made-up games (DEMO_DIR/insights/, answered at
// INSIGHTS_BASE, action/popular_flow's address) and the console's first
// games (DEMO_DIR/console/, put on the simulated console by seed_console()).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace web_demo
{

constexpr const char* DEMO_ORIGIN = "https://demo.playshelf.example";
constexpr const char* DEMO_DIR = "/app/samples/demo";
constexpr const char* INSIGHTS_BASE = "https://cdn.jsdelivr.net/gh/JigSawFr/PlayInsights@data/";

// At a start, before the console's games are read: when the page asked for
// them (playshelf_data/sim_console/seed_demo, written with the demo server
// on a first start), the demo's first games put on the simulated console
// (their content meta records, DEMO_DIR/console/meta/), the request gone.
void seed_console();

}   // namespace web_demo
