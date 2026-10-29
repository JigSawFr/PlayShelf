// web_demo — the web simulator's library server (web/README.md): its
// address, which source/sim/web_http.cpp answers from the files
// tools/web_demo.cpp and tools/web_demo_art.py made, packaged with the page
// at DEMO_DIR. The page adds it to servers.json on a first start
// (web/simulator.js). With it, the other services' answers about its
// made-up games, so the demo fetches nothing real: PlayInsights' files
// (DEMO_DIR/insights/, at INSIGHTS_BASE, action/popular_flow's address, its
// prices/ action/prices_flow's),
// Nlib API's details and pictures (DEMO_DIR/nlib/, at NLIB_BASE,
// titledb/nlib's) and a title database (DEMO_DIR/titledb/index.json, at
// TITLEDB_URL, NXMeta's in util/config.hpp); and the console's first games
// (DEMO_DIR/console/, put on the simulated console by seed_console()).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace web_demo
{

constexpr const char* DEMO_ORIGIN = "https://demo.playshelf.example";
constexpr const char* DEMO_DIR = "/app/samples/demo";
constexpr const char* INSIGHTS_BASE = "https://cdn.jsdelivr.net/gh/JigSawFr/PlayInsights@data/";
constexpr const char* NLIB_BASE = "https://api.nlib.cc/nx/";
constexpr const char* TITLEDB_URL = "https://nx-meta.nlib.cc/index.json";

// At a start, before the console's games are read: when the page asked for
// them (playshelf_data/sim_console/seed_demo, written with the demo server
// on a first start), the demo's first games put on the simulated console
// (their content meta records, DEMO_DIR/console/meta/; their names and
// icons, DEMO_DIR/console/control/), its wishlist in the data folder
// (DEMO_DIR/console/data/) and the title database of TITLEDB_URL fetched,
// the request gone.
void seed_console();

}   // namespace web_demo
