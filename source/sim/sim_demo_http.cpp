// The simulators' demo answers (sim/sim_demo.hpp): its library server's
// catalogue, packages and icons, and the other services' answers about its
// games, read from the demo's folder; every other address answered as a
// console without a connection.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sim/sim_demo.hpp"

#include <chrono>
#include <cstdio>
#include <sys/stat.h>
#include <thread>

#include "core/sim_env.h"
#include "sim/sim_net.hpp"

namespace sim_demo
{

const std::string& dir()
{
    static const std::string d = [] {
#ifdef __EMSCRIPTEN__
        const std::string at = "/app/samples/demo";
#else
        const std::string at = PLAYSHELF_DEMO_DIR;
#endif
        struct stat st;
        return !at.empty() && ::stat((at + "/index.json").c_str(), &st) == 0 ? at : std::string();
    }();
    return d;
}

namespace
{
constexpr size_t SLICE = 32u << 10;   // read, paced and handed on at a time

// The packaged file `url` names: on the demo server ("/" is its
// catalogue), or another service's answer about its games (sim_demo.hpp);
// false for any other address. *paced: the demo server's, served at the
// simulated speed; the others' at once.
bool demo_file(const std::string& url, std::string* path, bool* paced = nullptr)
{
    const std::string origin = DEMO_ORIGIN, insights = INSIGHTS_BASE, nlib = NLIB_BASE;
    std::string       rest;
    if (paced) *paced = false;
    if (dir().empty()) return false;
    if (url.compare(0, insights.size(), insights) == 0) rest = "/insights/" + url.substr(insights.size());
    else if (url == TITLEDB_URL) rest = "/titledb/index.json";
    else if (url.compare(0, nlib.size(), nlib) == 0 && url.size() >= nlib.size() + 16) {
        // <id>?lang=<language>&fields=…, <id>/icon/<size>, <id>/banner/480p,
        // <id>/screen/<n>: the demo's details in English or French, its
        // pictures.
        const std::string id = url.substr(nlib.size(), 16), what = url.substr(nlib.size() + 16);
        if (what.rfind("?lang=", 0) == 0) rest = "/nlib/" + id + "/details." + (what.compare(6, 2, "fr") == 0 ? "fr" : "en") + ".json";
        else if (what.rfind("/icon/", 0) == 0) rest = "/nlib/" + id + "/icon.jpg";
        else if (what == "/banner/480p") rest = "/nlib/" + id + "/banner.jpg";
        else if (what.rfind("/screen/", 0) == 0) rest = "/nlib/" + id + "/screen" + what.substr(8) + ".jpg";
        else return false;
    } else if (url.compare(0, origin.size(), origin) == 0) {
        rest = url.substr(origin.size());
        if (paced) *paced = true;
    } else
        return false;
    const size_t query = rest.find_first_of("?#");
    if (query != std::string::npos) rest.resize(query);
    if (rest.empty() || rest == "/") rest = "/index.json";
    if (rest.find("..") != std::string::npos) return false;
    *path = dir() + rest;
    return true;
}

bool size_of(const std::string& path, uint64_t* size)
{
    struct stat st;
    if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;
    *size = (uint64_t)st.st_size;
    return true;
}

// As long as `n` bytes take at the simulated speed.
void pace(size_t n)
{
    const long long kbs = sim_env_int("PLAYSHELF_SIM_SPEED", 96, 0, 1000000);
    if (kbs > 0) std::this_thread::sleep_for(std::chrono::microseconds((long long)n * 1000000 / (kbs * 1024)));
}

// [offset, offset + length) of `path` to `sink`, paced; false when it stops.
bool stream(const std::string& path, uint64_t offset, uint64_t length,
            const std::function<bool(const uint8_t*, size_t)>& sink, std::string* error, const std::atomic<bool>* cancel,
            bool paced = true)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f || std::fseek(f, (long)offset, SEEK_SET) != 0) {
        if (f) std::fclose(f);
        if (error) *error = "HTTP 404";
        return false;
    }
    std::vector<uint8_t> buf(SLICE);
    bool                 ok = true;
    while (length && ok) {
        if (cancel && cancel->load()) {
            if (error) *error = "cancelled";
            ok = false;
            break;
        }
        const size_t n = std::fread(buf.data(), 1, (size_t)std::min<uint64_t>(SLICE, length), f);
        if (!n) {
            if (error) *error = "the range came out short";
            ok = false;
            break;
        }
        if (paced) pace(n);
        if (!sink(buf.data(), n)) {
            if (error) *error = "stopped";
            ok = false;
        }
        length -= n;
    }
    std::fclose(f);
    return ok;
}

// The whole file, `max_bytes` at most; *status 200, or 404 for none.
bool whole(const std::string& path, std::string* body, std::string* error, size_t max_bytes, long* status, bool paced)
{
    uint64_t size = 0;
    if (!size_of(path, &size)) {
        if (status) *status = 404;
        if (error) *error = "HTTP 404";
        return false;
    }
    if (size > max_bytes) {
        if (error) *error = "the answer is too large";
        return false;
    }
    body->clear();
    if (!stream(path, 0, size, [body](const uint8_t* d, size_t n) { return body->append((const char*)d, n), true; }, error,
                nullptr, paced))
        return false;
    if (status) *status = 200;
    return true;
}
}   // namespace

bool serves(const std::string& url)
{
    std::string path;
    return demo_file(url, &path);
}

bool get(const std::string& url, std::string* body, std::string* error, size_t max_bytes, long, const http::Headers&,
         long* status)
{
    std::string path;
    bool        paced = true;
    if (!demo_file(url, &path, &paced)) return sim_net::offline(error);
    return whole(path, body, error, max_bytes, status, paced);
}

bool post(const std::string&, const std::string&, const char*, std::string*, std::string* error, long*, size_t, long,
          const http::Headers&)
{
    return sim_net::offline(error);
}

bool redirect(const std::string& url, std::string*, std::string* error, const http::Headers&, long* status)
{
    std::string path;
    bool        paced = true;
    if (!demo_file(url, &path, &paced)) return sim_net::offline(error);
    // The demo server answers itself: no redirect.
    if (status) *status = 200;
    return false;
}

bool download(const std::string& url, const std::string& path, std::string* error, size_t max_bytes, long,
              std::function<void(uint64_t, uint64_t)> progress, uint64_t resume_from, const std::atomic<bool>* cancel)
{
    std::string from;
    bool        paced = true;
    uint64_t    size = 0;
    if (!demo_file(url, &from, &paced)) return sim_net::offline(error);
    if (!size_of(from, &size)) {
        if (error) *error = "HTTP 404";
        return false;
    }
    if (size > max_bytes || resume_from > size) {
        if (error) *error = size > max_bytes ? "the answer is too large" : "HTTP 416";
        return false;
    }
    FILE* out = std::fopen(path.c_str(), resume_from ? "ab" : "wb");
    if (!out) {
        if (error) *error = "cannot create " + path;
        return false;
    }
    uint64_t done = resume_from;
    bool     ok = stream(from, resume_from, size - resume_from,
                     [&](const uint8_t* d, size_t n) {
                         if (std::fwrite(d, 1, n, out) != n) return false;
                         done += n;
                         if (progress) progress(done, size);
                         return true;
                     },
                     error, cancel, paced);
    if (std::fclose(out) != 0 && ok) {
        if (error) *error = "cannot write the file";
        ok = false;
    }
    return ok;
}

bool range(const std::string& url, uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink,
           std::string* error, const http::Headers&, const std::atomic<bool>* cancel, const std::string&)
{
    std::string path;
    bool        paced = true;
    uint64_t    size = 0;
    if (!demo_file(url, &path, &paced)) return sim_net::offline(error);
    if (!size_of(path, &size) || offset + length > size) {
        if (error) *error = "HTTP 416";
        return false;
    }
    return !length || stream(path, offset, length, sink, error, cancel, paced);
}

bool resource_size(const std::string& url, uint64_t* size, std::string* error, const http::Headers&, const std::string&)
{
    std::string path;
    bool        paced = true;
    if (!demo_file(url, &path, &paced)) return sim_net::offline(error);
    if (size_of(path, size)) return true;
    if (error) *error = "HTTP 404";
    return false;
}

bool request(const std::string& url, const http::Headers&, const std::string&, http::Response* response, std::string* error,
             size_t max_bytes, long)
{
    std::string path;
    bool        paced = true;
    if (!demo_file(url, &path, &paced)) return sim_net::offline(error);
    return whole(path, &response->body, error, max_bytes, &response->status, paced);
}

bool call(const std::string& url, const http::Call& c, const http::Headers&, const std::string&, http::Response* response,
          std::string* error, size_t max_bytes, long)
{
    std::string path;
    bool        paced = true;
    if (!demo_file(url, &path, &paced)) return sim_net::offline(error);
    response->cookies.clear();
    response->location.clear();
    // The demo server's sign-in always lets in (a session for its other
    // services); what it is sent is not kept: a form, a deletion.
    if (c.method == "POST" && url == std::string(DEMO_ORIGIN) + "/login") {
        response->status = 302;
        response->location = "/";
        response->cookies.push_back("session=demo");
        return true;
    }
    if (!c.method.empty() && c.method != "GET" && c.method != "OPTIONS") return sim_net::offline(error);
    if (c.to_path.empty()) return whole(path, &response->body, error, max_bytes, &response->status, paced);
    const bool ok = download(url, c.to_path, error, max_bytes, 0, c.progress, 0, c.cancel);
    response->status = ok ? 200 : 404;
    return ok;
}

bool peer_certificate(const std::string&, std::string*, std::string* error)
{
    return sim_net::offline(error);
}

}   // namespace sim_demo
