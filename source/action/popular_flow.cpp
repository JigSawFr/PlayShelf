// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/popular_flow.hpp"

#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <sys/stat.h>

#include <borealis.hpp>

#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

namespace popular_flow
{

namespace
{
constexpr const char* URL = "https://cdn.jsdelivr.net/gh/JigSawFr/PlayShelf@main/resources/data/popular.json";
constexpr size_t      MAX_BYTES = 2 * 1024 * 1024;
constexpr long        KEEP = 24 * 3600;   // fetched again a day after

std::shared_ptr<const library::PopularBook> s_book = std::make_shared<library::PopularBook>();
std::map<int, std::function<void()>>        s_listeners;
int                                         s_next = 1;
bool                                        s_fetching = false;

std::string cached_file()
{
    return paths::cache_dir() + "/popular.json";
}

// The cached copy's age, in seconds; -1 when there is none.
long age_of(const std::string& file)
{
    struct stat st;
    if (::stat(file.c_str(), &st) != 0) return -1;
    return (long)(std::time(nullptr) - st.st_mtime);
}

// On the UI thread: `book` in place of the one known, when it is newer.
void offer(std::shared_ptr<const library::PopularBook> book)
{
    if (!book || !book->size() || book->generated() < s_book->generated()) return;
    s_book = std::move(book);
    const auto listeners = s_listeners;
    for (const auto& l : listeners) l.second();
}

std::shared_ptr<library::PopularBook> read(const std::string& file)
{
    std::string text;
    auto        book = std::make_shared<library::PopularBook>();
    if (!paths::read_file(file, text) || !book->from_json(text)) return nullptr;
    return book;
}

// On a worker: the newest one, kept when it reads.
void fetch()
{
    http::Response r;
    std::string    why;
    if (!http::request(URL, {}, "", &r, &why, MAX_BYTES, 20) || r.status != 200) {
        brls::Logger::debug("popular: not fetched ({} {})", r.status, why);
        brls::sync([]() { s_fetching = false; });
        return;
    }
    auto book = std::make_shared<library::PopularBook>();
    if (!book->from_json(r.body)) {
        brls::Logger::warning("popular: the fetched file is not one");
        brls::sync([]() { s_fetching = false; });
        return;
    }
    if (paths::ensure_dir(paths::cache_dir())) paths::atomic_write(cached_file(), r.body);
    brls::Logger::info("popular: {} games, made {}", book->size(), book->generated());
    brls::sync([book]() {
        s_fetching = false;
        offer(book);
    });
}
}   // namespace

void start()
{
    const bool online = config::get().popular_online;
    s_fetching = online;
    ui::in_background("popular", [online]() {
        auto shipped = read(BRLS_ASSET("data/popular.json"));
        auto cached = read(cached_file());
        brls::sync([shipped, cached]() {
            offer(shipped);
            offer(cached);
        });
        const long age = age_of(cached_file());
        if (online && (age < 0 || age > KEEP)) fetch();
        else brls::sync([]() { s_fetching = false; });
    });
}

void refresh()
{
    if (s_fetching || !config::get().popular_online) return;
    s_fetching = true;
    ui::in_background("popular", []() { fetch(); });
}

library::Renown of(uint64_t base_id)
{
    return s_book->of(base_id);
}

size_t size()
{
    return s_book->size();
}

int listen(std::function<void()> changed)
{
    const int id = s_next++;
    s_listeners[id] = std::move(changed);
    return id;
}

void unlisten(int id)
{
    s_listeners.erase(id);
}

}   // namespace popular_flow
