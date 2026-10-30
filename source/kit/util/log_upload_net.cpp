// The network half of log_upload.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#include "app_id.h"
#ifndef __SWITCH__
#include "sim/sim_net.hpp"
#endif
#include "util/github_auth.hpp"
#include "util/http.hpp"
#include "util/log_upload.hpp"

namespace log_upload
{

namespace
{
// Desktop simulation: "offline", or the link the server answers. False on the console.
bool simulated(Result* r)
{
#ifndef __SWITCH__
    if (const char* sim = sim_net::value(APP_ENV "_SIM_PASTE")) {
        if (std::strcmp(sim, "offline") == 0) {
            sim_net::offline(&r->error);
            return true;
        }
        r->ok = true;
        r->url = sim;
        return true;
    }
#else
    (void)r;
#endif
    return false;
}
}   // namespace

Result upload_gist(const std::vector<Part>& files, const std::string& token, const std::string& version,
                   const std::string& what)
{
    Result r;
    if (simulated(&r)) return r;
    http::Headers headers = github_auth::api_headers(token);
    // The answer repeats the content: room for it, escaped.
    const auto got =
        http::post(github_auth::api_url("gists"), gist_body(files, version, what), "application/json", 3 * GIST_MAX_BYTES, 60, headers);
    if (!got) {
        const long status = got.error().status;
        // 401: the token was revoked (forgotten: no use keeping it); 403 /
        // 404: it was given before the app asked for the gist permission.
        r.relogin = github_auth::forget_if_revoked(status, token) || status == 403 || status == 404;
        r.error = "GitHub: " + got.error().error;
        return r;
    }
    if (!parse_gist(got->body, &r.url)) {
        r.error = "unexpected answer from GitHub (HTTP " + std::to_string(got->status) + ")";
        return r;
    }
    r.ok = true;
    return r;
}

}   // namespace log_upload
