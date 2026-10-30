// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/github_auth.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include <aurore/extern/nlohmann/json.hpp>

#include <aurore/cadran/util/text.hpp>

#include "util/json_util.hpp"
#include "util/paths.hpp"
#include "util/secret_box.hpp"

namespace github_auth
{

namespace
{
using nlohmann::json;

// GitHub tokens: letters, digits and '_' ("ghu_…"); nothing that could break a header.
bool token_chars(const std::string& t)
{
    return !t.empty() && t.size() <= 255 &&
           std::all_of(t.begin(), t.end(), [](char c) { return std::isalnum((unsigned char)c) || c == '_'; });
}
}   // namespace

bool parse_device_code(const std::string& text, DeviceCode* out)
{
    json j;
    if (!json_util::parse(text, &j) || !j.is_object()) return false;
    DeviceCode d;
    d.device_code = json_util::string_at(j, "device_code");
    d.user_code = json_util::string_at(j, "user_code");
    d.verification_uri = json_util::string_at(j, "verification_uri");
    d.interval = (int)json_util::integer_or(j, "interval", d.interval);
    d.expires_in = (int)json_util::integer_or(j, "expires_in", d.expires_in);
    if (d.device_code.empty() || d.user_code.empty() || !text::starts_with(d.verification_uri, "https://"))
        return false;
    d.interval = std::min(std::max(d.interval, 1), 60);
    d.expires_in = std::min(std::max(d.expires_in, 60), 3600);
    *out = d;
    return true;
}

Poll parse_poll(const std::string& text, std::string* token, std::string* error)
{
    json j;
    if (!json_util::parse(text, &j) || !j.is_object()) {
        if (error) *error = "unexpected answer";
        return Poll::Failed;
    }
    const std::string t = json_util::string_at(j, "access_token");
    if (!t.empty()) {
        if (!token_chars(t)) {
            if (error) *error = "unexpected token";
            return Poll::Failed;
        }
        *token = t;
        return Poll::Token;
    }
    const std::string e = json_util::string_at(j, "error");
    if (e == "authorization_pending") return Poll::Pending;
    if (e == "slow_down") return Poll::SlowDown;
    if (e == "expired_token") return Poll::Expired;
    if (e == "access_denied") return Poll::Denied;
    if (error) *error = e.empty() ? std::string("unexpected answer") : e;
    return Poll::Failed;
}

std::string token_file()
{
    return paths::data_dir() + "/github_token";
}

std::string token()
{
    std::string t;
    if (!paths::read_file(token_file(), t)) return "";
    t = text::trim_space_end(t);
    if (secret_box::is_sealed(t)) {
        // Sealed for this console (util/secret_box): another's reads as none.
        const auto opened = secret_box::open(t);
        t = opened ? *opened : "";
    } else if (!t.empty() && token_chars(t) && !text::starts_with(t, "gho_")) {
        // One an older version kept as it is: sealed now, where it lies.
        // Migration: goes from 2027-05 on, every install having read its
        // token once since.
        save_token(t, nullptr);
    }
    // The earlier OAuth app's token: refused the artifacts, signed in again.
    if (text::starts_with(t, "gho_")) return "";
    return token_chars(t) ? t : "";
}

bool save_token(const std::string& t, std::string* error)
{
    if (!token_chars(t)) {
        if (error) *error = "unexpected token";
        return false;
    }
    if (!paths::ensure_dir(paths::data_dir())) {
        if (error) *error = "cannot create " + paths::data_dir();
        return false;
    }
    const auto sealed = secret_box::seal(t);
    if (!sealed) {
        if (error) *error = "the console's key cannot be had: the token is not kept";
        return false;
    }
    return paths::atomic_write(token_file(), *sealed + "\n", error);
}

void forget_token()
{
    std::remove(token_file().c_str());
    std::remove((token_file() + ".tmp").c_str());
}

void forget_refused(const std::string& t)
{
    if (t.empty() || token() != t) return;
    forget_token();
}

bool forget_if_revoked(long status, const std::string& t)
{
    if (status != 401) return false;
    forget_refused(t);
    return true;
}

std::string api_url(const std::string& path)
{
    return API_ROOT + path;
}

std::vector<std::string> api_headers(const std::string& t)
{
    std::vector<std::string> h{ "Accept: application/vnd.github+json", "X-GitHub-Api-Version: 2022-11-28" };
    if (!t.empty()) h.push_back("Authorization: Bearer " + t);
    return h;
}

}   // namespace github_auth
