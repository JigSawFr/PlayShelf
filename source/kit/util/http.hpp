// http — a small HTTPS GET / POST / download on libcurl. On the Switch, devkitPro's switch-curl
// does TLS through the console's ssl service, which verifies servers against
// the system's built-in CA store; on desktop it is the system libcurl.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace http
{

// Extra request headers ("Authorization: Bearer …", "Accept: …").
using Headers = std::vector<std::string>;

// Follows redirects. False (and *error set, in English) on a network error,
// an HTTP status >= 400, a timeout or a body larger than `max_bytes`;
// *status gets the HTTP status when the server answered (a 404 tells
// "nothing there" from a network failure). Blocking: call it from brls::async.
// Requests on several threads run side by side.
bool get(const std::string& url, std::string* body, std::string* error,
         size_t max_bytes = 64 * 1024, long timeout_s = 10, const Headers& headers = {}, long* status = nullptr);

// POSTs `data` as `content_type` (no redirects followed). Same failures as
// get(); *status gets the HTTP status when the server answered (a 2xx other
// than 200, such as 201 or 206, is a success the caller may tell apart).
bool post(const std::string& url, const std::string& data, const char* content_type, std::string* body,
          std::string* error, long* status = nullptr, size_t max_bytes = 4 * 1024, long timeout_s = 30,
          const Headers& headers = {});

// Where `url` redirects to (an https:// URL), without following it: a
// signed download link can then be fetched without the headers (a token)
// sent to `url`. False when it does not redirect; *status gets the HTTP
// status when the server answered (as for get()).
bool redirect(const std::string& url, std::string* target, std::string* error, const Headers& headers = {},
              long* status = nullptr);

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
bool download(const std::string& url, const std::string& path, std::string* error, size_t max_bytes,
              long timeout_s = 600, std::function<void(uint64_t done, uint64_t total)> progress = nullptr,
              uint64_t resume_from = 0, const std::atomic<bool>* cancel = nullptr);

// At exit: a request still running stops at its next progress tick (about a
// second), and a new one fails at once, so the threads they run on can end
// before the app does.
void abort_all();

// Releases libcurl's global state (and the Switch ssl service). At exit;
// does nothing while a request is still running.
void cleanup();

}   // namespace http
