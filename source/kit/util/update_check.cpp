// The network half of update.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#ifndef __SWITCH__
#include "sim/sim_net.hpp"
#endif
#include "app.hpp"
#include "util/http.hpp"
#include "util/update.hpp"

namespace update
{

namespace
{
// <repository>/releases/latest/download/compat.json
std::string compat_url()
{
    return std::string(app::repo_url()) + "/releases/latest/download/compat.json";
}
}   // namespace

Result check(uint32_t console_fw)
{
    Result r;
    std::string body;
    long status = 0;
#ifndef __SWITCH__
    // Desktop simulation: "1.1.0:24.0.0" (latest version : firmware it supports),
    // "none" (no release published) or "offline".
    if (const char* sim = sim_net::value("PLAYSHELF_SIM_LATEST")) {
        if (std::strcmp(sim, "none") == 0) {
            r.verdict = Verdict::NoRelease;
            r.error = "HTTP 404";
            return r;
        }
        const char* colon = std::strchr(sim, ':');
        if (!colon) {
            sim_net::offline(&r.error);
            return r;
        }
        body = "{\"schema\": 1, \"version\": \"" + std::string(sim, colon) + "\", \"fw_tested_max\": \"" +
               std::string(colon + 1) + "\"}";
    } else
#endif
    if (!http::get(compat_url(), &body, &r.error, 64 * 1024, 10, {}, &status)) {
        // GitHub answers 404 for releases/latest while none is published.
        if (status == 404) r.verdict = Verdict::NoRelease;
        return r;
    }
    if (!parse_compat(body, &r.latest)) {
        r.error = "unexpected compat.json";
        return r;
    }
    r.verdict = decide(console_fw, app::version(), r.latest);
    return r;
}

}   // namespace update
