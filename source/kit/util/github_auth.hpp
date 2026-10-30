// github_auth — signs the app in to GitHub with the device flow, for the
// developer tools' development builds (dev_builds.hpp: GitHub hands Actions
// artifacts to signed-in users only) and the reports sent as gists.
//
// The app shows a short code and github.com/login/device (a QR code); the
// developer enters the code on a phone and authorises the app's "<app> dev
// builds" GitHub App; the app polls until GitHub hands it a token. The app
// asks for two permissions only: Actions (read), as GitHub hands artifacts
// only to a token allowed to read Actions, and Gists (write), so that "Send a
// report online" can make a secret gist (log_upload.hpp); otherwise the token
// can only read what is public. It is revoked from GitHub (Settings ›
// Applications › Authorized GitHub Apps) or by Disconnect here, which forgets
// it.
//
// The earlier OAuth app's tokens ("gho_…") are refused the artifacts: token()
// reads one as none, so the next opening asks to sign in again.
//
// The token is kept in its own file, sd:/config/<app>/github_token,
// sealed for this console (util/secret_box: one an older version kept as it
// is gets sealed when it is read),
// never in config.json, which "Send a report online" sends. It does not
// expire (the app has token expiry off: docs/contributing/releases.md), so GitHub hands
// no refresh token; one GitHub answers 401 (revoked, or the app's
// authorisation removed) is forgotten, and the next use asks to sign in.
//
// Parsing and the token file are plain C++ for the host tests
// (tests/dev_builds); start() and poll() are in github_auth_net.cpp.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace github_auth
{

// The GitHub App's client ID (public: the device flow has no secret).
constexpr const char* CLIENT_ID = "Iv23lipQVAoOXRkdiIc1";

struct DeviceCode
{
    std::string device_code;        // kept by the app, sent back when polling
    std::string user_code;          // "ABCD-1234", shown
    std::string verification_uri;   // "https://github.com/login/device"
    int         interval = 5;       // seconds between polls
    int         expires_in = 900;   // seconds the code is valid
};

enum class Poll
{
    Token,     // signed in: *token set
    Pending,   // not authorised yet: poll again after the interval
    SlowDown,  // too fast: the interval grows by 5 s
    Expired,   // the code expired: start again
    Denied,    // the developer refused
    Failed,    // anything else (*error set)
};

// POST https://github.com/login/device/code's answer.
bool parse_device_code(const std::string& json, DeviceCode* out);
// POST https://github.com/login/oauth/access_token's answer.
Poll parse_poll(const std::string& json, std::string* token, std::string* error);

// The saved token, or empty. A file holding anything but a token's
// characters, or an OAuth app's token ("gho_…"), reads as none.
std::string token();
bool        save_token(const std::string& token, std::string* error);
void        forget_token();
// forget_token(), when the saved token is still `token`: GitHub refused it
// (HTTP 401). A sign-in made since keeps its own.
void        forget_refused(const std::string& token);
// What every request made with `token` does with the HTTP status GitHub
// answered: a 401 (the token revoked) has it forgotten (forget_refused) and
// returns true; any other status, nothing (false).
bool        forget_if_revoked(long status, const std::string& token);
std::string token_file();

// The REST API's root; api_url("gists") -> "https://api.github.com/gists".
constexpr const char* API_ROOT = "https://api.github.com/";
std::string api_url(const std::string& path);

// "Authorization: Bearer <token>" plus the API's media type and version, for
// http::get; only the media type without a token.
std::vector<std::string> api_headers(const std::string& token);

// Asks GitHub for a code. Blocking (network). On the desktop build
// <APP>_SIM_GITHUB_LOGIN ("ok", "denied" or "offline") replaces it.
[[nodiscard]] bool start(DeviceCode* out, std::string* error);
// Asks whether the code was authorised. Blocking.
Poll poll(const DeviceCode& code, std::string* token, std::string* error);

}   // namespace github_auth
