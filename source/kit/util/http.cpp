// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/http.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

#include "app.hpp"
#include "app_id.h"
#include "util/http_logic.hpp"

namespace http
{

static std::atomic<bool> s_abort{ false };   // abort_all(): the app is quitting

namespace
{
// libcurl's global state, set up by the first request. Requests run
// side by side (a download of a few minutes holds up nothing else): what they
// share is the DNS cache and, where the TLS backend keeps them in libcurl,
// the TLS sessions (switch-curl's libnx backend keeps its own, inside the
// console's ssl service, per connection: there it is the DNS alone).
std::mutex        s_init_lock;       // held for the set-up only, never during a transfer
std::atomic<bool> s_ready{ false };
CURLSH*           s_share = nullptr;
std::mutex        s_share_locks[CURL_LOCK_DATA_LAST];
std::atomic<int>  s_running{ 0 };    // requests under way: cleanup() waits for none

void share_lock(CURL*, curl_lock_data data, curl_lock_access, void*)
{
    if ((unsigned)data < (unsigned)CURL_LOCK_DATA_LAST) s_share_locks[data].lock();
}

void share_unlock(CURL*, curl_lock_data data, void*)
{
    if ((unsigned)data < (unsigned)CURL_LOCK_DATA_LAST) s_share_locks[data].unlock();
}

// Set up by the first request (again by the next one when it failed).
bool ready()
{
    if (s_ready) return true;
    std::lock_guard<std::mutex> guard(s_init_lock);
    if (s_ready) return true;
    // On the Switch this starts the ssl and csrng services (switch-curl's libnx backend).
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return false;
    // Without the share, each request simply resolves and shakes hands on its own.
    s_share = curl_share_init();
    if (s_share && (curl_share_setopt(s_share, CURLSHOPT_LOCKFUNC, share_lock) != CURLSHE_OK ||
                    curl_share_setopt(s_share, CURLSHOPT_UNLOCKFUNC, share_unlock) != CURLSHE_OK ||
                    curl_share_setopt(s_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS) != CURLSHE_OK)) {
        curl_share_cleanup(s_share);
        s_share = nullptr;
    }
    // A nicety: refused, the DNS still is shared.
    if (s_share) curl_share_setopt(s_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    s_ready = true;   // last: a request that sees it sees the share too
    return true;
}

// Counts a request under way for as long as it lives.
struct Running
{
    Running()  { s_running++; }
    ~Running() { s_running--; }
};
}   // namespace

namespace
{
struct Sink
{
    std::string* body;             // the response in memory, or…
    size_t       max;
    bool         overflow = false;
    FILE*        file = nullptr;   // … written to this file
    size_t       written = 0;
    bool         file_error = false;
    std::function<void(uint64_t, uint64_t)> progress{};
    CURL*        curl = nullptr;
    uint64_t     base = 0;          // bytes already in the file (a download resumed)
    bool         status_seen = false;
    bool         refused = false;   // the answer was not written (http_logic::download_start)
    const std::atomic<bool>* cancel = nullptr;   // set: the transfer stops (download())
    // range(): each piece to `chunk` as it arrives, once the answer is a 206
    // for the range asked (`range_from`, `range_length`).
    const std::function<bool(const uint8_t*, size_t)>* chunk = nullptr;
    uint64_t                 range_from = 0, range_length = 0, range_got = 0;
    bool                     chunk_stopped = false;   // `chunk` said stop
    std::string              range_error{};
    bool                     has_range = false;
    http_logic::ContentRange content_range{};   // from the answer's headers
    std::vector<std::string>* cookies = nullptr;   // call(): the Set-Cookie headers' "name=value"
    std::function<void(uint64_t, uint64_t)> sent{};   // call(): a file's bytes sent so far
};

bool cancelled(const Sink* sink)
{
    return sink && sink->cancel && sink->cancel->load();
}

// Called by curl about once a second, even while waiting for the server.
int on_progress(void* user, curl_off_t, curl_off_t, curl_off_t ul_total, curl_off_t ul_now)
{
    const auto* sink = static_cast<const Sink*>(user);
    if (sink && sink->sent && ul_total > 0) sink->sent((uint64_t)ul_now, (uint64_t)ul_total);
    return s_abort || cancelled(sink) ? 1 : 0;   // non-zero aborts the transfer
}

// Each header line of the answer (and of a redirect's): the Content-Range kept.
size_t on_header(char* data, size_t size, size_t count, void* user)
{
    auto* sink = static_cast<Sink*>(user);
    const size_t n = size * count;
    http_logic::ContentRange r;
    if (http_logic::parse_content_range(std::string(data, n), &r)) {
        sink->content_range = r;
        sink->has_range = true;
    }
    if (sink->cookies) {
        std::string cookie;
        if (http_logic::parse_set_cookie(std::string(data, n), &cookie)) sink->cookies->push_back(cookie);
    }
    return n;
}

size_t on_data(char* data, size_t size, size_t count, void* user)
{
    auto* sink = static_cast<Sink*>(user);
    if (s_abort || cancelled(sink)) return 0;
    const size_t n = size * count;
    if (sink->chunk) {
        if (!sink->status_seen) {
            sink->status_seen = true;
            long status = 0;
            curl_easy_getinfo(sink->curl, CURLINFO_RESPONSE_CODE, &status);
            if (!http_logic::range_ok(status, sink->has_range ? &sink->content_range : nullptr, sink->range_from,
                                      sink->range_length, &sink->range_error)) {
                sink->refused = true;
                return 0;
            }
        }
        if (n > sink->range_length - sink->range_got) {
            sink->overflow = true;
            return 0;
        }
        sink->range_got += n;
        if (!(*sink->chunk)(reinterpret_cast<const uint8_t*>(data), n)) {
            sink->chunk_stopped = true;
            return 0;
        }
        return n;
    }
    // A download looks at the status before its first byte: an error page is
    // not written (a part kept stays usable), a resumed download the server
    // answers in full (200, not 206) starts over, from an empty file.
    if (sink->file && !sink->status_seen) {
        sink->status_seen = true;
        long status = 0;
        curl_easy_getinfo(sink->curl, CURLINFO_RESPONSE_CODE, &status);
        switch (http_logic::download_start(status, sink->base > 0)) {
            case http_logic::Start::Refuse:
                sink->refused = true;
                return 0;   // aborts the transfer
            case http_logic::Start::StartOver:
                std::fflush(sink->file);
                if (::ftruncate(::fileno(sink->file), 0) != 0) {
                    sink->file_error = true;
                    return 0;
                }
                std::rewind(sink->file);
                sink->base = 0;
                break;
            case http_logic::Start::Write:
                break;
        }
    }
    const size_t have = sink->file ? sink->base + sink->written : sink->body->size();
    if (!http_logic::fits(have, n, sink->max)) {
        sink->overflow = true;
        return 0;   // aborts the transfer (CURLE_WRITE_ERROR)
    }
    if (!sink->file) {
        sink->body->append(data, n);
        return n;
    }
    if (std::fwrite(data, 1, n, sink->file) != n) {
        sink->file_error = true;
        return 0;
    }
    sink->written += n;
    if (sink->progress) {
        curl_off_t total = -1;
        curl_easy_getinfo(sink->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
        sink->progress(sink->base + sink->written, total > 0 ? sink->base + (uint64_t)total : 0);
    }
    return n;
}
}   // namespace

namespace
{
// What a request adds to a plain GET.
struct Extra
{
    const std::string* post = nullptr;           // POST this body…
    const char*        content_type = nullptr;   // … as this type
    const Headers*     headers = nullptr;
    bool               follow = true;            // follow redirects (never for a POST)
    std::string*       location = nullptr;       // the redirect target, when not followed
    Sink*              to_file = nullptr;        // replaces the in-memory sink (download, range)
    std::string        range;                    // "first-last": a Range request
    const std::string* pinned_key = nullptr;     // "sha256//…": this key, not the CA store, vouches for the server
    std::string*       peer_pem = nullptr;       // peer_certificate(): the server's certificate, unverified
    bool*              tls_failed = nullptr;     // set when the server's certificate or key was refused
    const Call*        call = nullptr;           // call(): its method and form
    std::vector<std::string>* cookies = nullptr;  // call(): the cookies the answer sets
};

bool perform(const std::string& url, const Extra& x, std::string* body, std::string* error, size_t max_bytes,
             long timeout_s, long* status_out)
{
    const std::string* post = x.post;
    Sink* to_file = x.to_file;
    const Running running;
    body->clear();
    if (status_out) *status_out = 0;
    if (s_abort) {
        if (error) *error = APP_NAME " is quitting";
        return false;
    }
    if (!ready()) {
        if (error) *error = "libcurl initialisation failed";
        return false;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) *error = "libcurl initialisation failed";
        return false;
    }

    Sink  own{ body, max_bytes };
    Sink& sink = to_file ? *to_file : own;
    sink.curl = curl;
    if (x.call) {
        sink.cookies = x.cookies;
        sink.cancel = x.call->cancel;
        if (!x.call->file_path.empty()) sink.sent = x.call->progress;
    }
    const std::string agent = APP_NAME "/" + app::version();
    char message[CURL_ERROR_SIZE] = "";
    struct curl_slist* headers = nullptr;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (s_share) curl_easy_setopt(curl, CURLOPT_SHARE, s_share);
    // Another method than GET or POST, or a form: what call() asks.
    const std::string method = x.call ? x.call->method : std::string();
    const bool        custom = !method.empty() && method != "GET" && method != "POST";
    const bool        form = x.call && (!x.call->fields.empty() || !x.call->file_path.empty());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, x.follow && !post && !custom && !form ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    // HTTPS only, redirects included (no downgrade to http:// or other schemes).
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
#endif
    // The server's certificate and name are checked (libcurl's defaults,
    // written out so that no build or backend change can turn them off),
    // unless a pinned key vouches for it instead (a LAN server's own
    // certificate: its key must be that one), or peer_certificate() only
    // reads it, to make a pin the user is asked about.
    const bool pinned = x.pinned_key && !x.pinned_key->empty();
    const bool vouched = pinned || x.peer_pem;
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, vouched ? 0L : 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, vouched ? 0L : 2L);
    if (pinned) curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, x.pinned_key->c_str());
    if (x.peer_pem) {
        curl_easy_setopt(curl, CURLOPT_CERTINFO, 1L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, std::min(timeout_s, 30L));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);
    if (!x.range.empty()) curl_easy_setopt(curl, CURLOPT_RANGE, x.range.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, on_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sink);
    if (to_file || (form && !x.call->file_path.empty())) {   // a long transfer: give up on a stall rather than at the end
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        // A file sent: the server may take its time over it before it answers.
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, to_file ? 30L : 180L);
        if (to_file && to_file->base && !to_file->chunk)
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)to_file->base);
    }
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &sink);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, message);
    if (post) {
        // Never followed (above): a redirect must not turn the POST into a GET elsewhere.
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post->data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)post->size());
    }
    curl_mime* mime = nullptr;
    if (form) {
        // Its fields, then its file, read from the disk as it is sent.
        mime = curl_mime_init(curl);
        for (const auto& [name, value] : x.call->fields) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, name.c_str());
            curl_mime_data(part, value.data(), value.size());
        }
        if (!x.call->file_path.empty()) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, x.call->file_field.c_str());
            curl_mime_filedata(part, x.call->file_path.c_str());
            curl_mime_filename(part, x.call->file_name.c_str());
            curl_mime_type(part, "application/octet-stream");
        }
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    }
    if (custom) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    for (const std::string& h : http_logic::request_headers(post ? x.content_type : nullptr, x.headers))
        headers = curl_slist_append(headers, h.c_str());
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (x.tls_failed)
        *x.tls_failed = rc == CURLE_PEER_FAILED_VERIFICATION || rc == CURLE_SSL_PINNEDPUBKEYNOTMATCH ||
                        rc == CURLE_SSL_CACERT_BADFILE || rc == CURLE_SSL_CERTPROBLEM;
    if (x.peer_pem) {
        x.peer_pem->clear();
        struct curl_certinfo* info = nullptr;
        if (curl_easy_getinfo(curl, CURLINFO_CERTINFO, &info) == CURLE_OK && info && info->num_of_certs > 0)
            for (struct curl_slist* l = info->certinfo[0]; l; l = l->next)
                if (std::strncmp(l->data, "Cert:", 5) == 0) *x.peer_pem = l->data + 5;
    }
    if (x.location) {
        char* target = nullptr;
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &target);
        *x.location = target ? target : "";
    }
    curl_easy_cleanup(curl);
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    if (status_out) *status_out = status;

    http_logic::Outcome outcome;
    outcome.status      = status;
    outcome.overflow    = sink.overflow;
    outcome.file_error  = sink.file_error;
    outcome.refused     = sink.refused;
    outcome.transfer_ok = rc == CURLE_OK;
    outcome.range_error = rc == CURLE_RANGE_ERROR;
    if (rc != CURLE_OK) outcome.transfer_error = message[0] ? message : curl_easy_strerror(rc);
    if (sink.chunk) {
        // A range: its own reasons first, then what the transfer says.
        if (!sink.range_error.empty()) {
            if (error) *error = sink.range_error;
            return false;
        }
        if (sink.chunk_stopped) {
            if (error) *error = "stopped";
            return false;
        }
        if (!http_logic::succeeded(outcome, error)) return false;
        if (sink.range_got != sink.range_length) {
            if (error) *error = "the range came out short";
            return false;
        }
        return true;
    }
    return http_logic::succeeded(outcome, error);
}
#ifndef __SWITCH__
std::atomic<const StandIn*> s_stand_in{ nullptr };

// The stand-in, when it answers `url`.
const StandIn* stand_in(const std::string& url)
{
    const StandIn* s = s_stand_in.load();
    return s && s->serves(url) ? s : nullptr;
}
#endif
}   // namespace

#ifndef __SWITCH__
void set_stand_in(const StandIn* answers)
{
    s_stand_in = answers;
}
#endif

namespace
{
Result<Reply> reply_of(const std::string& url, const Extra& x, size_t max_bytes, long timeout_s)
{
    Reply       r;
    std::string error;
    if (!perform(url, x, &r.body, &error, max_bytes, timeout_s, &r.status))
        return std::unexpected(Failure{ std::move(error), r.status });
    return r;
}
}   // namespace

Result<Reply> get(const std::string& url, size_t max_bytes, long timeout_s, const Headers& headers)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url)) return s->get(url, max_bytes, timeout_s, headers);
#endif
    Extra x;
    x.headers = &headers;
    return reply_of(url, x, max_bytes, timeout_s);
}

Result<Reply> post(const std::string& url, const std::string& data, const char* content_type, size_t max_bytes,
                   long timeout_s, const Headers& headers)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url)) return s->post(url, data, content_type, max_bytes, timeout_s, headers);
#endif
    Extra x;
    x.post = &data;
    x.content_type = content_type;
    x.headers = &headers;
    return reply_of(url, x, max_bytes, timeout_s);
}

Result<std::string> redirect(const std::string& url, const Headers& headers)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url)) return s->redirect(url, headers);
#endif
    std::string target;
    Extra       x;
    x.headers = &headers;
    x.follow = false;
    x.location = &target;
    std::string body, error;
    long        answered = 0;
    if (!perform(url, x, &body, &error, 64 * 1024, 30, &answered) || !http_logic::redirect_ok(answered, target, &error))
        return std::unexpected(Failure{ std::move(error), answered });
    return target;
}

Result<void> download(const std::string& url, const std::string& path, size_t max_bytes, long timeout_s,
                      std::function<void(uint64_t, uint64_t)> progress, uint64_t resume_from,
                      const std::atomic<bool>* cancel)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url))
        return s->download(url, path, max_bytes, timeout_s, std::move(progress), resume_from, cancel);
#endif
    // Owner-writable only (fopen would ask for 0666): an executable lands here.
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | (resume_from ? O_APPEND : O_TRUNC), 0644);
    FILE* file = fd >= 0 ? ::fdopen(fd, resume_from ? "ab" : "wb") : nullptr;
    if (!file) {
        if (fd >= 0) ::close(fd);
        return std::unexpected(Failure{ "cannot create " + path });
    }
    std::string unused;
    Sink sink{ &unused, max_bytes };
    sink.file = file;
    sink.base = resume_from;
    sink.progress = std::move(progress);
    sink.cancel = cancel;
    Extra x;
    x.to_file = &sink;
    Failure failed;
    bool    ok = perform(url, x, &unused, &failed.error, max_bytes, timeout_s, &failed.status);
    if (cancel && cancel->load()) {
        failed.error = "cancelled";
        ok = false;
    }
    if (std::fclose(file) != 0 && ok) {
        failed.error = "cannot write the file";
        ok = false;
    }
    if (!ok) return std::unexpected(std::move(failed));
    return {};
}

Result<void> range(const std::string& url, uint64_t offset, uint64_t length,
                   const std::function<bool(const uint8_t*, size_t)>& sink, const Headers& headers,
                   const std::atomic<bool>* cancel, const std::string& pinned_key)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url)) return s->range(url, offset, length, sink, headers, cancel, pinned_key);
#endif
    if (!length) return {};
    std::string unused;
    Sink        s{ &unused, (size_t)-1 };
    s.chunk = &sink;
    s.range_from = offset;
    s.range_length = length;
    s.cancel = cancel;
    Extra x;
    x.headers = &headers;
    x.to_file = &s;
    x.range = std::to_string(offset) + "-" + std::to_string(offset + length - 1);
    x.pinned_key = &pinned_key;
    Failure failed;
    bool    ok = perform(url, x, &unused, &failed.error, (size_t)-1, 0, &failed.status);
    if (cancel && cancel->load()) {
        failed.error = "cancelled";
        ok = false;
    }
    if (!ok) return std::unexpected(std::move(failed));
    return {};
}

Result<uint64_t> resource_size(const std::string& url, const Headers& headers, const std::string& pinned_key)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url)) return s->resource_size(url, headers, pinned_key);
#endif
    uint64_t           got = 0;
    const std::function<bool(const uint8_t*, size_t)> one = [&](const uint8_t*, size_t n) {
        got += n;
        return true;
    };
    std::string unused;
    Sink        s{ &unused, (size_t)-1 };
    s.chunk = &one;
    s.range_length = 1;
    Extra x;
    x.headers = &headers;
    x.to_file = &s;
    x.range = "0-0";
    x.pinned_key = &pinned_key;
    Failure failed;
    if (!perform(url, x, &unused, &failed.error, 16, 30, &failed.status)) return std::unexpected(std::move(failed));
    return s.content_range.total;
}

bool request(const std::string& url, const Headers& headers, const std::string& pinned_key, Response* r,
             std::string* error, size_t max_bytes, long timeout_s)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url)) return s->request(url, headers, pinned_key, r, error, max_bytes, timeout_s);
#endif
    Extra x;
    x.headers = &headers;
    x.pinned_key = &pinned_key;
    x.tls_failed = &r->tls_failed;
    r->tls_failed = false;
    return perform(url, x, &r->body, error, max_bytes, timeout_s, &r->status);
}

bool call(const std::string& url, const Call& c, const Headers& headers, const std::string& pinned_key, Response* r,
          std::string* error, size_t max_bytes, long timeout_s)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url)) return s->call(url, c, headers, pinned_key, r, error, max_bytes, timeout_s);
#endif
    Extra x;
    x.headers = &headers;
    x.pinned_key = &pinned_key;
    x.tls_failed = &r->tls_failed;
    x.call = &c;
    x.cookies = &r->cookies;
    x.follow = c.follow;
    if (!c.body.empty() || (c.method == "POST" && c.fields.empty() && c.file_path.empty())) {
        x.post = &c.body;
        x.content_type = c.content_type.empty() ? nullptr : c.content_type.c_str();
    }
    std::string location;
    if (!c.follow) x.location = &location;
    r->tls_failed = false;
    r->cookies.clear();
    r->location.clear();
    if (c.to_path.empty()) {
        const bool ok = perform(url, x, &r->body, error, max_bytes, timeout_s, &r->status);
        r->location = location;
        return ok;
    }
    // Owner-writable only, as download()'s.
    const int fd = ::open(c.to_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE* file = fd >= 0 ? ::fdopen(fd, "wb") : nullptr;
    if (!file) {
        if (fd >= 0) ::close(fd);
        if (error) *error = "cannot create " + c.to_path;
        return false;
    }
    std::string unused;
    Sink        sink{ &unused, max_bytes };
    sink.file = file;
    sink.progress = c.progress;
    x.to_file = &sink;
    bool ok = perform(url, x, &unused, error, max_bytes, timeout_s, &r->status);
    r->body.clear();
    r->location = location;
    if (c.cancel && c.cancel->load()) {
        if (error) *error = "cancelled";
        ok = false;
    }
    if (std::fclose(file) != 0 && ok) {
        if (error) *error = "cannot write the file";
        ok = false;
    }
    return ok;
}

Result<std::string> peer_certificate(const std::string& url)
{
#ifndef __SWITCH__
    if (const StandIn* s = stand_in(url)) return s->peer_certificate(url);
#endif
    std::string pem;
    Extra       x;
    x.peer_pem = &pem;
    std::string body;
    // The answer itself does not matter (a 401, a 404): only the handshake.
    perform(url, x, &body, nullptr, 64 * 1024, 15, nullptr);
    if (pem.find("-----BEGIN CERTIFICATE-----") == std::string::npos)
        return std::unexpected(Failure{ "the server's certificate cannot be read" });
    return pem;
}

void abort_all()
{
    s_abort = true;
}

void cleanup()
{
    // A request still running (the app quit during a check) would hold the
    // exit for up to its timeout: skip the cleanup, the process ends anyway.
    // Called once, at exit, after abort_all(): no request starts after it.
    if (s_running > 0 || !s_ready) return;
    if (s_share) curl_share_cleanup(s_share);
    s_share = nullptr;
    curl_global_cleanup();
    s_ready = false;
}

}   // namespace http
