// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/library_icons.hpp"

#include <borealis.hpp>
#include <deque>
#include <mutex>
#include <set>

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

struct Job
{
    std::shared_ptr<const library_flow::Loaded> loaded;
    std::string                                 url;
    std::string                                 password;
    std::function<void(int)>                    done;
    uint64_t                                    title_id = 0;   // the game's: the title database's icon, then Nlib API's
    bool                                        online = false; // config online_icons when it was asked
};

// An image's pixels, decoded off the UI thread when NanoVG cannot (WebP):
// made a texture on it.
struct Image
{
    std::string          bytes;   // PNG, JPEG: NanoVG's to read
    std::vector<uint8_t> rgba;
    int                  width = 0, height = 0;
};

Image image_of(std::string bytes, const std::string& ext)
{
    Image out;
    if (ext != "webp") {
        out.bytes = std::move(bytes);
        return out;
    }
    int      w = 0, h = 0;
    uint8_t* px = WebPDecodeRGBA((const uint8_t*)bytes.data(), bytes.size(), &w, &h);
    if (px && w > 0 && h > 0 && w <= 1024 && h <= 1024) {
        out.rgba.assign(px, px + (size_t)w * (size_t)h * 4);
        out.width = w;
        out.height = h;
    }
    WebPFree(px);
    return out;
}

int texture_of(const Image& img)
{
    NVGcontext* vg = brls::Application::getNVGContext();
    if (!img.rgba.empty()) return nvgCreateImageRGBA(vg, img.width, img.height, 0, img.rgba.data());
    if (img.bytes.empty()) return 0;
    return nvgCreateImageMem(vg, 0, (unsigned char*)img.bytes.data(), (int)img.bytes.size());
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
        brls::Logger::debug("library: no icon from {} ({} {})", url, r.status, why);
        return "";
    }
    if (paths::ensure_dir(paths::cache_dir()) && paths::ensure_dir(dir())) paths::atomic_write(file, r.body);
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
Image local_image(const Job& job)
{
    std::vector<uint8_t> bytes;
    std::string          ext;
    if (titledb_flow::icon(job.title_id, &bytes, &ext)) return image_of(std::string(bytes.begin(), bytes.end()), ext);
    const uint64_t    base = library::base_of(job.title_id);
    const std::string url = job.online ? titledb::nlib_icon_url(base) : "";
    if (url.empty()) return {};
    {
        std::lock_guard<std::mutex> hold(s_lock);
        if (s_unknown.count(base)) return {};
    }
    std::string data = fetched(url, {}, "", 10);
    if (data.empty()) {
        std::lock_guard<std::mutex> hold(s_lock);
        s_unknown.insert(base);
    }
    return image_of(std::move(data), "");
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
        auto img = std::make_shared<Image>(job.loaded ? image_of(bytes_of(job), "") : Image{});
        if (img->bytes.empty() && job.title_id) *img = local_image(job);
        auto done = job.done;
        brls::sync([img, done]() { done(texture_of(*img)); });
    }
}
}   // namespace

void get(std::shared_ptr<const library_flow::Loaded> l, const std::string& password, const std::string& url,
         std::function<void(int)> done, uint64_t title_id)
{
    if (!l || url.empty()) return;
    Job  job{ std::move(l), url, password, std::move(done), title_id, config::get().online_icons };
    bool start = false;
    {
        std::lock_guard<std::mutex> hold(s_lock);
        s_jobs.push_back(std::move(job));
        if (!s_running) start = s_running = true;
    }
    if (start) ui::in_background("library icons", drain);
}

void local(uint64_t title_id, std::function<void(int)> done)
{
    if (!title_id) return;
    Job job;
    job.title_id = title_id;
    job.online = config::get().online_icons;
    job.done = std::move(done);
    bool start = false;
    {
        std::lock_guard<std::mutex> hold(s_lock);
        s_jobs.push_back(std::move(job));
        if (!s_running) start = s_running = true;
    }
    if (start) ui::in_background("library icons", drain);
}

void cached(const std::string& url, const std::function<void(int)>& done)
{
    if (url.empty()) {
        done(0);
        return;
    }
    const std::string file = file_of(url);
    ui::in_background("cached icon", [file, done]() {
        auto data = std::make_shared<std::string>();
        if (!paths::read_file(file, *data)) data->clear();
        brls::sync([data, done]() {
            const int texture = data->empty() ? 0
                : nvgCreateImageMem(brls::Application::getNVGContext(), 0, (unsigned char*)data->data(), (int)data->size());
            done(texture);
        });
    });
}

void drop_pending()
{
    std::lock_guard<std::mutex> hold(s_lock);
    s_jobs.clear();
}

}   // namespace library_icons
