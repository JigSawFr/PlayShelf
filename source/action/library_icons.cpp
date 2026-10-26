// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/library_icons.hpp"

#include <borealis.hpp>
#include <deque>
#include <mutex>

#include "library/servers.hpp"
#include "ui/ui.hpp"
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
};

std::mutex       s_lock;
std::deque<Job>  s_jobs;
bool             s_running = false;

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

// On the worker: the icon's bytes, from the cache or the server.
std::string bytes_of(const Job& job)
{
    const std::string file = file_of(job.url);
    std::string       data;
    if (paths::read_file(file, data) && !data.empty()) return data;
    const library::Server& s = job.loaded->server;
    const bool             own = library::same_origin(job.url, job.loaded->fetch.endpoint);
    http::Response         r;
    std::string            why;
    const http::Headers    headers = own ? library::auth_headers(s.username, job.password) : http::Headers();
    if (!http::request(job.url, headers, own && job.loaded->fetch.on_lan ? s.pin : "", &r, &why, MAX_ICON, 15) ||
        r.status != 200 || r.body.empty()) {
        brls::Logger::debug("library: no icon from {} ({} {})", job.url, r.status, why);
        return "";
    }
    if (paths::ensure_dir(paths::cache_dir()) && paths::ensure_dir(dir())) paths::atomic_write(file, r.body);
    return r.body;
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
        auto data = std::make_shared<std::string>(bytes_of(job));
        auto done = job.done;
        brls::sync([data, done]() {
            int texture = 0;
            if (!data->empty())
                texture = nvgCreateImageMem(brls::Application::getNVGContext(), 0, (unsigned char*)data->data(),
                                            (int)data->size());
            done(texture);
        });
    }
}
}   // namespace

void get(std::shared_ptr<const library_flow::Loaded> l, const std::string& password, const std::string& url,
         std::function<void(int)> done)
{
    if (!l || url.empty()) return;
    Job  job{ std::move(l), url, password, std::move(done) };
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
