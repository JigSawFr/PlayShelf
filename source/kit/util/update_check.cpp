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
// <repository>/releases/latest/download/compat-<schema>.json, or the
// compat.json every release has ("" for it).
std::string compat_url(const std::string& schema)
{
    return std::string(app::repo_url()) + "/releases/latest/download/compat" + (schema.empty() ? "" : "-" + schema) + ".json";
}
}   // namespace

Result check(uint32_t console_fw)
{
    Result r;
    std::string body;
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
    {
        // This build's schema first; a release without that file (published
        // before it was) answers 404, and has compat.json.
        auto got = http::get(compat_url(std::to_string(COMPAT_SCHEMA)), 64 * 1024, 10);
        if (!got && got.error().status == 404) got = http::get(compat_url(""), 64 * 1024, 10);
        if (!got) {
            r.error = got.error().error;
            // GitHub answers 404 for releases/latest while none is published.
            if (got.error().status == 404) r.verdict = Verdict::NoRelease;
            return r;
        }
        body = std::move(got->body);
    }
    if (!parse_compat(body, &r.latest)) {
        r.error = "unexpected compat.json";
        return r;
    }
    r.verdict = decide(console_fw, app::version(), r.latest);
    return r;
}

}   // namespace update
