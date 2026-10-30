// http_logic — the decisions behind http, without libcurl: the size limit of a
// response, the request headers, how a transfer's outcome becomes the error
// the caller shows, what a download does with the answer it gets, and which
// answers count as a redirect, a range answer's Content-Range. Plain C++ so the
// host tests can run it (tests/http_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace http_logic
{

// `n` more bytes after `have` stay within `max`; past it the transfer stops.
bool fits(size_t have, size_t n, size_t max);

// The headers a request sends, in order: for a POST (`content_type` set) its
// type and an empty "Expect:" (no 100-continue round trip), then `extra`.
std::vector<std::string> request_headers(const char* content_type, const std::vector<std::string>* extra);

// What the transfer ended with -> success, or false and *error (English).
// The status comes first: a refusal often comes as a large HTML page, so it
// says the status, not its size. `transfer_error` is libcurl's message when
// `transfer_ok` is false.
struct Outcome
{
    long        status = 0;           // HTTP status, 0 when the server did not answer
    bool        overflow = false;     // the body went past the size limit
    bool        file_error = false;   // the download could not be written
    bool        refused = false;      // a download's answer not written (download_start)
    bool        transfer_ok = true;   // CURLE_OK
    bool        range_error = false;  // CURLE_RANGE_ERROR: a Range request the server would not honour
    std::string transfer_error;
};
[[nodiscard]] bool succeeded(const Outcome& o, std::string* error);

// The error of a resumed download the server will not go on with, the same
// whatever libcurl's version says ("doesn't seem to support byte ranges" in
// 7.69.1, the Switch's, "does not seem…" in 8.x): libcurl stops a Range
// request answered in full (200) before its first byte reaches the file.
extern const char* const RANGE_REFUSED;

// A resumed download to fetch again from the start: the part kept no longer
// matches what the server has (416), or the server cannot resume it
// (RANGE_REFUSED). `error` as succeeded() wrote it.
bool restart_resume(const std::string& error);

// What a download does with the first bytes of the answer, from its status.
// `resumed`: the file already holds the start of it, and the rest was asked
// for (a Range request). An error page (>= 400) is never written, so a part
// kept for a later attempt stays as it was; a resumed download goes on with
// a 206, starts over with a 200 (the server sends it all again) and keeps
// its part on anything else.
enum class Start
{
    Write,       // append to the file as it is
    StartOver,   // empty the file first
    Refuse,      // write nothing, stop the transfer
};
Start download_start(long status, bool resumed);

// A redirect() answer: a 3xx status pointing to an https:// URL. False and
// *error otherwise.
[[nodiscard]] bool redirect_ok(long status, const std::string& target, std::string* error);

// A response header line "Content-Range: bytes <first>-<last>/<total>"
// (the name in any case, spaces around the value): the range a 206 holds
// and the resource's size. False for any other line, a "*" total, or a
// range that does not fit in the total.
struct ContentRange
{
    uint64_t first = 0, last = 0, total = 0;
};
bool parse_content_range(const std::string& line, ContentRange* out);

// A "Set-Cookie:" header line (the name in any case): its "name=value",
// without the attributes after it (Path, Secure…). False for any other line,
// or a cookie without a name or with a control character.
bool parse_set_cookie(const std::string& line, std::string* cookie);

// What a range() answer may be: a 206 for exactly the range asked.
[[nodiscard]] bool range_ok(long status, const ContentRange* got, uint64_t first, uint64_t length, std::string* error);

}   // namespace http_logic
