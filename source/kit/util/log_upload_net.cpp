// The network half of log_upload.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

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
    if (const char* sim = sim_net::value("PLAYSHELF_SIM_PASTE")) {
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
    std::string body;
    long status = 0;
    http::Headers headers = github_auth::api_headers(token);
    // The answer repeats the content: room for it, escaped.
    if (!http::post(github_auth::api_url("gists"), gist_body(files, version, what), "application/json", &body, &r.error,
                    &status, 3 * GIST_MAX_BYTES, 60, headers)) {
        // 401: the token was revoked (forgotten: no use keeping it); 403 /
        // 404: it was given before PlayShelf asked for the gist permission.
        r.relogin = github_auth::forget_if_revoked(status, token) || status == 403 || status == 404;
        r.error = "GitHub: " + r.error;
        return r;
    }
    if (!parse_gist(body, &r.url)) {
        r.error = "unexpected answer from GitHub (HTTP " + std::to_string(status) + ")";
        return r;
    }
    r.ok = true;
    return r;
}

}   // namespace log_upload
