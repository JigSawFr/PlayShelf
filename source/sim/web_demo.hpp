// web_demo — the web simulator's library server (web/README.md): its
// address, which source/sim/web_http.cpp answers from the files
// tools/web_demo.cpp made, packaged with the page at DEMO_DIR. The page adds
// it to servers.json on a first start (web/simulator.js).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace web_demo
{

constexpr const char* DEMO_ORIGIN = "https://demo.playshelf.example";
constexpr const char* DEMO_DIR = "/app/samples/demo";

}   // namespace web_demo
