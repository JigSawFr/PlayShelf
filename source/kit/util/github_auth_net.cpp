// The network half of github_auth.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#include "app_id.h"
#ifndef __SWITCH__
#include "sim/sim_net.hpp"
#endif

#include <aurore/cadran/util/text.hpp>

#include "util/github_auth.hpp"
#include "util/http.hpp"

namespace github_auth
{

namespace
{
const http::Headers JSON{ "Accept: application/json" };

#ifndef __SWITCH__
const char* sim()
{
    return sim_net::value(APP_ENV "_SIM_GITHUB_LOGIN");
}
#endif
}   // namespace

bool start(DeviceCode* out, std::string* error)
{
    std::string body;
#ifndef __SWITCH__
    if (const char* s = sim()) {
        if (std::strcmp(s, "offline") == 0) return sim_net::offline(error);
        body = R"({"device_code": "sim", "user_code": "SIMU-1234", "verification_uri": "https://github.com/login/device",
                   "interval": 1, "expires_in": 900})";
    } else
#endif
    {
        auto got = http::post("https://github.com/login/device/code", "client_id=" + std::string(CLIENT_ID),
                              "application/x-www-form-urlencoded", 8 * 1024, 30, JSON);
        if (!got) {
            if (error) *error = got.error().error;
            return false;
        }
        body = std::move(got->body);
    }
    if (!parse_device_code(body, out)) {
        if (error) *error = "unexpected answer from GitHub";
        return false;
    }
    return true;
}

Poll poll(const DeviceCode& code, std::string* token, std::string* error)
{
    std::string body;
#ifndef __SWITCH__
    if (const char* s = sim()) {
        body = std::strcmp(s, "denied") == 0 ? R"({"error": "access_denied"})"
                                             : R"({"access_token": "ghu_simulated", "token_type": "bearer"})";
    } else
#endif
    {
        auto got = http::post("https://github.com/login/oauth/access_token",
                              "client_id=" + std::string(CLIENT_ID) + "&device_code=" + text::url_encode(code.device_code) +
                                  "&grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code",
                              "application/x-www-form-urlencoded", 8 * 1024, 30, JSON);
        if (!got) {
            if (error) *error = got.error().error;
            return Poll::Failed;
        }
        body = std::move(got->body);
    }
    return parse_poll(body, token, error);
}

}   // namespace github_auth
