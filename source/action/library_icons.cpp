// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/library_icons.hpp"

#include <aurore.hpp>
#include <algorithm>
#include <deque>
#include <mutex>
#include <set>

#include <aurore/extern/nanovg/stb_image.h>
#include <webp/decode.h>

#include "action/titledb_flow.hpp"
#include "library/catalog.hpp"
#include "library/servers.hpp"
#include "titledb/nlib.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"

namespace library_icons
{
namespace
{
constexpr size_t MAX_ICON = 1024 * 1024;
constexpr int    MAX_SIDE = 4096;   // a JPEG or PNG larger than this is not decoded

struct Job
{
    std::shared_ptr<const library_flow::Loaded> loaded;
    std::string                                 url;
    std::string                                 password;
    Done                                        done;
    uint64_t                                    title_id = 0;   // the game's: the title database's icon, then Nlib API's
    bool                                        online = false; // config nlib_api when it was asked
    int                                         max_w = 0, max_h = 0;   // a picture (picture()): its bounds
};

// Off the UI thread: a JPEG or PNG (`ext` "webp": a WebP of 1024 × 1024 at
// most) decoded, nullptr when it does not. stb_image gives the pixels NanoVG
// would have made of the file itself.
Picture decoded(const uint8_t* data, size_t size, const std::string& ext = "")
{
    if (!data || !size) return nullptr;
    auto out = std::make_shared<Pixels>();
    int  w = 0, h = 0, n = 0;
    if (ext == "webp") {
        uint8_t* px = WebPDecodeRGBA(data, size, &w, &h);
        if (px && w > 0 && h > 0 && w <= 1024 && h <= 1024) out->rgba.assign(px, px + (size_t)w * (size_t)h * 4);
        WebPFree(px);
    } else {
        uint8_t* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
        if (px && w > 0 && h > 0 && w <= MAX_SIDE && h <= MAX_SIDE) out->rgba.assign(px, px + (size_t)w * (size_t)h * 4);
        stbi_image_free(px);
    }
    if (out->rgba.empty()) return nullptr;
    out->width = w;
    out->height = h;
    return out;
}

Picture decoded(const std::string& bytes)
{
    return decoded((const uint8_t*)bytes.data(), bytes.size());
}

// A JPEG or PNG decoded and made at most max_w × max_h, each pixel of the
// result the mean of those it covers; nullptr when it does not decode.
Picture scaled(const std::string& bytes, int max_w, int max_h)
{
    const Picture full = decoded(bytes);
    if (!full) return nullptr;
    const int    w = full->width, h = full->height;
    const double scale = std::min({ 1.0, (double)max_w / w, (double)max_h / h });
    const int    ow = std::max(1, (int)(w * scale)), oh = std::max(1, (int)(h * scale));
    if (ow == w && oh == h) return full;
    const uint8_t* px = full->rgba.data();
    auto           out = std::make_shared<Pixels>();
    out->rgba.resize((size_t)ow * (size_t)oh * 4);
    for (int y = 0; y < oh; y++) {
        const int y0 = y * h / oh, y1 = std::max(y0 + 1, (y + 1) * h / oh);
        for (int x = 0; x < ow; x++) {
            const int x0 = x * w / ow, x1 = std::max(x0 + 1, (x + 1) * w / ow);
            uint32_t  sum[4] = { 0, 0, 0, 0 };
            for (int sy = y0; sy < y1; sy++)
                for (int sx = x0; sx < x1; sx++)
                    for (int k = 0; k < 4; k++) sum[k] += px[((size_t)sy * w + sx) * 4 + k];
            const uint32_t count = (uint32_t)((y1 - y0) * (x1 - x0));
            for (int k = 0; k < 4; k++) out->rgba[((size_t)y * ow + x) * 4 + k] = (uint8_t)(sum[k] / count);
        }
    }
    out->width = ow;
    out->height = oh;
    return out;
}

std::mutex         s_lock;
std::deque<Job>    s_jobs;
bool               s_running = false;
std::set<uint64_t> s_unknown;   // the games Nlib API had no icon for (s_lock)

std::string dir()
{
    return paths::cache_dir() + "/library_icons";
}

std::string file_of(const std::string& url)
{
    Sha256 h;
    h.update(url.data(), url.size());
    return dir() + "/" + h.hex().substr(0, 32) + ".img";
}

// On the worker: the bytes at `url`, from the cache, else fetched (1 MB
// at most) and kept there; "" when none came.
std::string fetched(const std::string& url, const http::Headers& headers, const std::string& pin, int timeout)
{
    const std::string file = file_of(url);
    std::string       data;
    if (paths::read_file(file, data) && !data.empty()) return data;
    http::Response r;
    std::string    why;
    if (!http::request(url, headers, pin, &r, &why, MAX_ICON, timeout) || r.status != 200 || r.body.empty()) {
        aurore::Logger::debug("library: no icon from {} ({} {})", url, r.status, why);
        return "";
    }
    ui::keep_copy("library icon", file, r.body);
    return r.body;
}

// On the worker: the server's icon (the credentials and the pin only for
// its own address).
std::string bytes_of(const Job& job)
{
    const library::Server& s = job.loaded->server;
    const bool             own = library::same_origin(job.url, job.loaded->fetch.endpoint);
    const http::Headers    headers = own ? library::auth_headers(s.username, job.password) : http::Headers();
    return fetched(job.url, headers, own && job.loaded->fetch.on_lan ? s.pin : "", 15);
}

// On the worker: the game's icon from the title database, else from Nlib
// API (a game it did not know left alone).
Picture local_picture(const Job& job)
{
    std::vector<uint8_t> bytes;
    std::string          ext;
    if (titledb_flow::icon(job.title_id, &bytes, &ext)) return decoded(bytes.data(), bytes.size(), ext);
    const uint64_t    base = library::base_of(job.title_id);
    const std::string url = job.online ? titledb::nlib_icon_url(base) : "";
    if (url.empty()) return nullptr;
    {
        std::lock_guard<std::mutex> hold(s_lock);
        if (s_unknown.contains(base)) return nullptr;
    }
    std::string data = fetched(url, {}, "", 10);
    if (data.empty()) {
        std::lock_guard<std::mutex> hold(s_lock);
        s_unknown.insert(base);
    }
    return decoded(data);
}

void drain()
{
    for (;;) {
        Job job;
        {
            std::lock_guard<std::mutex> hold(s_lock);
            if (s_jobs.empty() || ui::quitting()) {
                s_running = false;
                return;
            }
            job = std::move(s_jobs.front());
            s_jobs.pop_front();
        }
        Picture picture;
        if (job.max_w) picture = scaled(fetched(job.url, {}, "", 15), job.max_w, job.max_h);
        else {
            // The server's icon; the game's own when the server gave none.
            const std::string bytes = job.loaded ? bytes_of(job) : "";
            picture = bytes.empty() && job.title_id ? local_picture(job) : decoded(bytes);
        }
        aurore::sync([picture, done = std::move(job.done)]() { done(picture); });
    }
}

void queue(Job job)
{
    bool start = false;
    {
        std::lock_guard<std::mutex> hold(s_lock);
        s_jobs.push_back(std::move(job));
        if (!s_running) start = s_running = true;
    }
    if (start) ui::in_background("library icons", drain);
}
}   // namespace

bool show(aurore::Image* image, const Picture& picture)
{
    if (!image || !picture) return false;
    image->setImageFromRGBA(picture->rgba.data(), picture->width, picture->height);
    return true;
}

void get(std::shared_ptr<const library_flow::Loaded> l, const std::string& password, const std::string& url, Done done,
         uint64_t title_id)
{
    if (!l || url.empty()) return;
    Job job;
    job.loaded = std::move(l);
    job.url = url;
    job.password = password;
    job.done = std::move(done);
    job.title_id = title_id;
    job.online = config::get().nlib_api;
    queue(std::move(job));
}

void local(uint64_t title_id, Done done)
{
    if (!title_id) return;
    Job job;
    job.title_id = title_id;
    job.online = config::get().nlib_api;
    job.done = std::move(done);
    queue(std::move(job));
}

void picture(const std::string& url, int max_w, int max_h, Done done)
{
    if (url.empty() || max_w <= 0 || max_h <= 0) return;
    Job job;
    job.url = url;
    job.max_w = max_w;
    job.max_h = max_h;
    job.done = std::move(done);
    queue(std::move(job));
}

void cached(const std::string& url, const Done& done)
{
    if (url.empty()) {
        done(nullptr);
        return;
    }
    const std::string file = file_of(url);
    ui::in_background("cached icon", [file, done]() {
        std::string data;
        if (!paths::read_file(file, data)) data.clear();
        aurore::sync([picture = decoded(data), done]() { done(picture); });
    });
}

void decode(std::shared_ptr<const std::vector<uint8_t>> bytes, Done done)
{
    if (!bytes) {
        done(nullptr);
        return;
    }
    aurore::async([bytes = std::move(bytes), done = std::move(done)]() {
        aurore::sync([picture = decoded(bytes->data(), bytes->size()), done]() { done(picture); });
    });
}

void drop_pending()
{
    std::lock_guard<std::mutex> hold(s_lock);
    std::erase_if(s_jobs, [](const Job& j) { return !j.max_w; });
}

}   // namespace library_icons
