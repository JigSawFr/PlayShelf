// The network half of update.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#include "app_id.h"
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
// <repository>/releases/latest/download/<name>
std::string compat_url(const std::string& name)
{
    return std::string(app::repo_url()) + "/releases/latest/download/" + name;
}
}   // namespace

Result check(uint32_t console_fw)
{
    Result r;
    std::string body;
#ifndef __SWITCH__
    // Desktop simulation: "1.1.0:24.0.0" (latest version : firmware it supports),
    // "none" (no release published) or "offline".
    if (const char* sim = sim_net::value(APP_ENV "_SIM_LATEST")) {
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
    {
        // The document in this version's schema; compat.json (schema 1, kept
        // for every version) when the release has none or it cannot be read.
        const std::string versioned = "compat-" + std::to_string(COMPAT_SCHEMA) + ".json";
        auto got = http::get(compat_url(versioned), 64 * 1024, 10);
        if (!got && got.error().status != 404) {
            r.error = got.error().error;
            return r;
        }
        if (got) body = std::move(got->body);
        if (!got || !parse_compat(body, &r.latest)) {
            auto fallback = http::get(compat_url("compat.json"), 64 * 1024, 10);
            if (!fallback) {
                r.error = fallback.error().error;
                // GitHub answers 404 for releases/latest while none is published.
                if (fallback.error().status == 404) r.verdict = Verdict::NoRelease;
                return r;
            }
            body = std::move(fallback->body);
        }
    }
    if (!parse_compat(body, &r.latest)) {
        r.error = "unexpected compat.json";
        return r;
    }
    r.verdict = decide(console_fw, app::version(), r.latest);
    return r;
}

}   // namespace update
