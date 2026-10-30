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

bool get(const std::string& url, std::string* body, std::string* error, size_t max_bytes, long timeout_s,
         const Headers& headers, long* status)
{
    return sim_demo::get(url, body, error, max_bytes, timeout_s, headers, status);
}

bool post(const std::string& url, const std::string& data, const char* content_type, std::string* body,
          std::string* error, long* status, size_t max_bytes, long timeout_s, const Headers& headers)
{
    return sim_demo::post(url, data, content_type, body, error, status, max_bytes, timeout_s, headers);
}

bool redirect(const std::string& url, std::string* target, std::string* error, const Headers& headers, long* status)
{
    return sim_demo::redirect(url, target, error, headers, status);
}

bool download(const std::string& url, const std::string& path, std::string* error, size_t max_bytes, long timeout_s,
              std::function<void(uint64_t, uint64_t)> progress, uint64_t resume_from, const std::atomic<bool>* cancel)
{
    return sim_demo::download(url, path, error, max_bytes, timeout_s, std::move(progress), resume_from, cancel);
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
