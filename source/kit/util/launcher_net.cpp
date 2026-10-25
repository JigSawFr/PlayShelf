// The network half of launcher.hpp: the version the Homebrew App Store's
// catalogue holds (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#ifndef __SWITCH__
#include "sim/sim_net.hpp"
#endif
#include "util/http.hpp"
#include "util/json_util.hpp"
#include "util/launcher.hpp"

namespace launcher
{

namespace
{
// The catalogue's copy of the listing's info.json (switch-hbas-repo builds it
// with spinarak, which publishes it next to the package): a 404 until
// PlayShelf is listed.
constexpr const char* STORE_INFO_URL = "https://switch.cdn.fortheusers.org/packages/PlayShelf/info.json";
}   // namespace

std::string store_version(std::string* error)
{
    std::string body;
    long status = 0;
#ifndef __SWITCH__
    // Desktop simulation: "1.2.0" (the version the catalogue has), "none"
    // (PlayShelf not listed) or "offline".
    if (const char* sim = sim_net::value("PLAYSHELF_SIM_STORE")) {
        if (std::strcmp(sim, "none") == 0) {
            if (error) *error = "not listed";
            return "";
        }
        if (std::strcmp(sim, "offline") == 0) {
            sim_net::offline(error);
            return "";
        }
        body = "{\"version\": \"" + std::string(sim) + "\"}";
    } else
#endif
    {
        std::string err;
        if (!http::get(STORE_INFO_URL, &body, &err, 16 * 1024, 10, {}, &status)) {
            if (error) *error = status == 404 ? "not listed" : err;
            return "";
        }
    }
    nlohmann::json info;
    std::string version;
    if (json_util::parse(body, &info)) version = json_util::string_at(info, "version");
    if (version.empty() && error) *error = "unexpected info.json";
    return version;
}

}   // namespace launcher
