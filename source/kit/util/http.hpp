// http — a small HTTPS GET / POST / download on libcurl. On the Switch, devkitPro's switch-curl
// does TLS through the console's ssl service, which verifies servers against
// the system's built-in CA store; on desktop it is the system libcurl.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <vector>

namespace http
{

// Extra request headers ("Authorization: Bearer …", "Accept: …").
using Headers = std::vector<std::string>;

// A request that failed: why, in English, and the HTTP status when the
// server answered (0 otherwise: a 404 tells "nothing there" from a network
// failure).
struct Failure
{
    std::string error;
    long        status = 0;
};

// What the server answered: the body, and the HTTP status (a 2xx other than
// 200, such as 201 or 206, is a success the caller may tell apart).
struct Reply
{
    std::string body;
    long        status = 0;
};

template <class T> using Result = std::expected<T, Failure>;

// Follows redirects. A Failure on a network error, an HTTP status >= 400, a
// timeout or a body larger than `max_bytes`. Blocking: call it from
// aurore::async. Requests on several threads run side by side.
[[nodiscard]] Result<Reply> get(const std::string& url, size_t max_bytes = 64 * 1024, long timeout_s = 10,
                                const Headers& headers = {});

// POSTs `data` as `content_type` (no redirects followed). Same failures as get().
[[nodiscard]] Result<Reply> post(const std::string& url, const std::string& data, const char* content_type,
                                 size_t max_bytes = 4 * 1024, long timeout_s = 30, const Headers& headers = {});

// Where `url` redirects to (an https:// URL), without following it: a
// signed download link can then be fetched without the headers (a token)
// sent to `url`. A Failure when it does not redirect.
[[nodiscard]] Result<std::string> redirect(const std::string& url, const Headers& headers = {});

// GETs `url` (redirects followed) into the file `path`, created or
// truncated. Fails beyond `max_bytes`, or when nothing arrives for 30 s;
// `progress` (when set) gets the bytes received so far and the total
// announced (0 when unknown), from the download thread. The file is left
// behind on failure: the caller removes it, or resumes it.
// `resume_from` > 0: `path` already holds that many bytes of it; the rest is
// asked for (a Range request) and appended. A server that sends it all again
// (200, not 206) has the file started over, unless libcurl stops it first
// (http_logic::RANGE_REFUSED, the file as it was); an error answer (>= 400), or
// any other one to a Range request, writes nothing, and the file keeps what
// it held.
// `cancel` (when given) set by another thread stops the transfer within a
// second ("cancelled").
[[nodiscard]] Result<void> download(const std::string& url, const std::string& path, size_t max_bytes, long timeout_s = 600,
                                    std::function<void(uint64_t done, uint64_t total)> progress = nullptr,
                                    uint64_t resume_from = 0, const std::atomic<bool>* cancel = nullptr);

// GETs bytes [offset, offset + length) of `url` (a Range request,
// redirects followed) and hands them to `sink` as they arrive, in order.
// The server must answer 206 with exactly that range: a 200 (no ranges) or
// another range fails before any byte reaches `sink`. Fails when `sink`
// returns false ("stopped"), when nothing arrives for 30 s, or when the
// range comes out short; no overall timeout. `cancel` as for download().
// `pinned_key` ("sha256//<base64>", as CURLOPT_PINNEDPUBLICKEY takes it): the
// server must hold that public key, which vouches for it in place of the CA
// store (a LAN server's own certificate); empty, the CA store does.
Result<void> range(const std::string& url, uint64_t offset, uint64_t length,
                   const std::function<bool(const uint8_t*, size_t)>& sink, const Headers& headers = {},
                   const std::atomic<bool>* cancel = nullptr, const std::string& pinned_key = "");

// The size of `url`'s resource, from the Content-Range of a request for its
// first byte: a Failure when the server does not serve ranges.
[[nodiscard]] Result<uint64_t> resource_size(const std::string& url, const Headers& headers = {},
                                             const std::string& pinned_key = "");

// A GET as get() does, its pinned key as range()'s, and whether the TLS
// handshake failed on the server's certificate or key (tls_failed): a LAN
// server's own certificate, never vouched for, that a pin may be made for.
struct Response
{
    long        status = 0;
    std::string body;
    bool        tls_failed = false;
    // call(): the cookies the answer sets ("name=value", without their
    // attributes), and where it redirects to when the redirect is not followed.
    std::vector<std::string> cookies;
    std::string              location;
};
bool request(const std::string& url, const Headers& headers, const std::string& pinned_key, Response* response,
             std::string* error, size_t max_bytes = 4 * 1024 * 1024, long timeout_s = 30);

// A request beyond a GET, made as request() makes one (its pinned key, its
// TLS failure told): its method, a body sent as it is or a form (multipart/
// form-data: its fields, then a file's part read from the disk as it goes),
// the answer's body kept in memory or written to a file.
struct Call
{
    std::string method;                  // "" a GET (a POST when it has a body or a form); "DELETE", "OPTIONS"…
    std::string body, content_type;      // sent as it is
    std::vector<std::pair<std::string, std::string>> fields;   // multipart/form-data
    std::string file_field, file_path, file_name;               // its file's part, when `file_path` is set
    std::string to_path;                 // the answer's body into this file (created or truncated), not Response::body
    bool        follow = true;           // redirects followed: a GET's only, never a POST's
    const std::atomic<bool>* cancel = nullptr;                   // set by another thread: stops within a second
    // The bytes sent so far and their total (a file sent), else received and
    // the total announced (0 when unknown); from the request's thread.
    std::function<void(uint64_t done, uint64_t total)> progress = nullptr;
};
// False as request() is (a status >= 400 included, the status told); a file
// sent or received gives up on 30 s without a byte rather than on
// `timeout_s` (0: no overall limit).
bool call(const std::string& url, const Call& c, const Headers& headers, const std::string& pinned_key, Response* response,
          std::string* error, size_t max_bytes = 4 * 1024 * 1024, long timeout_s = 30);

// The certificate `url`'s server presents, as PEM, without checking it and
// without sending anything but the request line (no header, no
// credential): what a pin is made from, which the user is asked about
// before it is trusted. A Failure when the TLS backend cannot give it.
[[nodiscard]] Result<std::string> peer_certificate(const std::string& url);

// At exit: a request still running stops at its next progress tick (about a
// second), and a new one fails at once, so the threads they run on can end
// before the app does.
void abort_all();

// Releases libcurl's global state (and the Switch ssl service). At exit;
// does nothing while a request is still running.
void cleanup();

#ifndef __SWITCH__
// Not the console (the simulator): what answers some addresses in place of
// the network, without libcurl (a demo the app plays, when it has one).
// `serves` says which; each of the others answers as the function of its
// name above. Set once, before the first request; nullptr: the network.
struct StandIn
{
    bool (*serves)(const std::string& url);
    decltype(&http::get)              get;
    decltype(&http::post)             post;
    decltype(&http::redirect)         redirect;
    decltype(&http::download)         download;
    decltype(&http::range)            range;
    decltype(&http::resource_size)    resource_size;
    decltype(&http::request)          request;
    decltype(&http::call)             call;
    decltype(&http::peer_certificate) peer_certificate;
};
void set_stand_in(const StandIn* answers);
#endif

}   // namespace http
