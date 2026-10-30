// The web simulator's http (web/README.md): no libcurl in a browser, and a
// page cannot reach most servers without their CORS headers, so every
// request is answered as a console without a connection, but those the demo
// answers (sim/sim_demo.hpp: its library server, Atlas, Nlib API, a
// title database). The PLAYSHELF_SIM_* folders the *_net.cpp files read
// still answer first.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/http.hpp"

#include "sim/sim_demo.hpp"

namespace http
{

Result<Reply> get(const std::string& url, size_t max_bytes, long timeout_s, const Headers& headers)
{
    return sim_demo::get(url, max_bytes, timeout_s, headers);
}

Result<Reply> post(const std::string& url, const std::string& data, const char* content_type, size_t max_bytes,
                   long timeout_s, const Headers& headers)
{
    return sim_demo::post(url, data, content_type, max_bytes, timeout_s, headers);
}

Result<std::string> redirect(const std::string& url, const Headers& headers)
{
    return sim_demo::redirect(url, headers);
}

Result<void> download(const std::string& url, const std::string& path, size_t max_bytes, long timeout_s,
                      std::function<void(uint64_t, uint64_t)> progress, uint64_t resume_from, const std::atomic<bool>* cancel)
{
    return sim_demo::download(url, path, max_bytes, timeout_s, std::move(progress), resume_from, cancel);
}

bool range(const std::string& url, uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
           std::string* error, const Headers& headers, const std::atomic<bool>* cancel, const std::string& pinned_key)
{
    return sim_demo::range(url, offset, length, sink, error, headers, cancel, pinned_key);
}

bool resource_size(const std::string& url, uint64_t* size, std::string* error, const Headers& headers,
                   const std::string& pinned_key)
{
    return sim_demo::resource_size(url, size, error, headers, pinned_key);
}

bool request(const std::string& url, const Headers& headers, const std::string& pinned_key, Response* response,
             std::string* error, size_t max_bytes, long timeout_s)
{
    return sim_demo::request(url, headers, pinned_key, response, error, max_bytes, timeout_s);
}

bool call(const std::string& url, const Call& c, const Headers& headers, const std::string& pinned_key, Response* response,
          std::string* error, size_t max_bytes, long timeout_s)
{
    return sim_demo::call(url, c, headers, pinned_key, response, error, max_bytes, timeout_s);
}

bool peer_certificate(const std::string& url, std::string* pem, std::string* error)
{
    return sim_demo::peer_certificate(url, pem, error);
}

void abort_all() {}

void cleanup() {}

}   // namespace http
