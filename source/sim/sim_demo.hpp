// sim_demo — the simulators' demo (web/README.md,
// docs/contributing/simulator.md): a library server of made-up games, at
// DEMO_ORIGIN, which source/sim/sim_demo_http.cpp answers from the files
// tools/web_demo.cpp and tools/web_demo_art.py made into dir(); and the
// other services' answers about its games, so the demo fetches nothing
// real: PlayInsights' files (dir()/insights/, at INSIGHTS_BASE,
// action/popular_flow's address, its prices/ action/prices_flow's), Nlib API's details and pictures
// (dir()/nlib/, at NLIB_BASE, titledb/nlib's) and a title database
// (dir()/titledb/index.json, at TITLEDB_URL, NXMeta's in util/config.hpp);
// and the console's first games (dir()/console/, put on the simulated
// console by start() on a first start).
// The web simulator packages the folder with the page; the desktop one reads
// it where `make desktop` made it (PLAYSHELF_DEMO_DIR, CMakeLists.txt),
// its other addresses on the network (util/http's stand-in).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include "util/http.hpp"

namespace sim_demo
{

constexpr const char* DEMO_ORIGIN = "https://demo.playshelf.example";
constexpr const char* INSIGHTS_BASE = "https://cdn.jsdelivr.net/gh/JigSawFr/PlayInsights@data/";
constexpr const char* NLIB_BASE = "https://api.nlib.cc/nx/";
constexpr const char* TITLEDB_URL = "https://nx-meta.nlib.cc/index.json";

// The demo's folder: "" when this build has none (`make desktop
// DESKTOP_DEMO=`), or it is not there.
const std::string& dir();

// Whether the demo answers `url`: one of the addresses above, the demo
// built.
bool serves(const std::string& url);

// The demo's answers, as util/http's functions of the same names give
// theirs (each "simulated: no connection" for an address it does not
// serve): its server's at PLAYSHELF_SIM_SPEED KB/s (96 unless set; 0: at
// once), so a download is seen going, the other services' at once.
[[nodiscard]] bool get(const std::string& url, std::string* body, std::string* error, size_t max_bytes, long timeout_s,
                       const http::Headers& headers, long* status);
bool post(const std::string& url, const std::string& data, const char* content_type, std::string* body,
          std::string* error, long* status, size_t max_bytes, long timeout_s, const http::Headers& headers);
[[nodiscard]] bool redirect(const std::string& url, std::string* target, std::string* error, const http::Headers& headers,
                            long* status);
[[nodiscard]] bool download(const std::string& url, const std::string& path, std::string* error, size_t max_bytes, long timeout_s,
                            std::function<void(uint64_t, uint64_t)> progress, uint64_t resume_from, const std::atomic<bool>* cancel);
bool range(const std::string& url, uint64_t offset, uint64_t length,
           const std::function<bool(const uint8_t*, size_t)>& sink, std::string* error, const http::Headers& headers,
           const std::atomic<bool>* cancel, const std::string& pinned_key);
[[nodiscard]] bool resource_size(const std::string& url, uint64_t* size, std::string* error, const http::Headers& headers,
                                 const std::string& pinned_key);
bool request(const std::string& url, const http::Headers& headers, const std::string& pinned_key,
             http::Response* response, std::string* error, size_t max_bytes, long timeout_s);
bool call(const std::string& url, const http::Call& c, const http::Headers& headers, const std::string& pinned_key,
          http::Response* response, std::string* error, size_t max_bytes, long timeout_s);
[[nodiscard]] bool peer_certificate(const std::string& url, std::string* pem, std::string* error);

// At a start, before the servers and the console's games are read, the
// demo built: on the desktop, its answers in place of the network's
// (http::set_stand_in). Then, unless PLAYSHELF_SIM_NO_DEMO: its packages,
// signed with the host tests' key, installed as a console's would
// (PLAYSHELF_SIM_SIGNED, unless set; the web page sets it itself); and on a
// first start (no servers.json yet):
// its server in servers.json, sys-patch on the simulated SD card (its
// packages install without the question a console with no patches gets),
// its first games on the simulated console (their content meta records,
// dir()/console/meta/; their names and icons, dir()/console/control/), its
// wishlist in the data folder (dir()/console/data/) and the title database
// of TITLEDB_URL fetched.
void start();

}   // namespace sim_demo
