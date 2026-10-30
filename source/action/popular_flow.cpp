// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/popular_flow.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <string>

#include <aurore.hpp>

#include "action/insights_index.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"

namespace popular_flow
{

namespace
{
// The Switch's files, in PlayInsights' switch/ folder.
constexpr const char* BASE = "https://cdn.jsdelivr.net/gh/JigSawFr/PlayInsights@data/";
constexpr const char* FOLDER = "switch/";
constexpr size_t      MAX_BYTES = 4 * 1024 * 1024;
constexpr long        KEEP = 24 * 3600;   // fetched again a day after

// One of PlayInsights' files: the book read from it, the newest kept. Its
// name is the one shipped and cached; in PlayInsights it is in FOLDER.
template <typename Book>
struct File
{
    const char*                 name;
    std::shared_ptr<const Book> book = std::make_shared<Book>();

    std::string cached() const { return paths::cache_dir() + "/" + name; }
    std::string published() const { return std::string(FOLDER) + name; }
};

File<library::PopularBook> s_popular{ "popular.json" };
File<library::SignalBook>  s_signals{ "games.json" };
File<library::SimilarBook> s_similar{ "similar.json" };
File<library::EditionBook> s_editions{ "editions.json" };
std::map<int, std::function<void()>> s_listeners;
int                                  s_next = 1;
bool                                 s_fetching = false;

void tell()
{
    const auto listeners = s_listeners;
    for (const auto& l : listeners) l.second();
}

template <typename Book>
std::shared_ptr<Book> parse(const std::string& text)
{
    auto book = std::make_shared<Book>();
    return book->from_json(text) ? book : nullptr;
}

template <typename Book>
std::shared_ptr<Book> read(const std::string& path)
{
    std::string text;
    return paths::read_file(path, text) ? parse<Book>(text) : nullptr;
}

// On the UI thread: `book` in place of the one known, when it is newer.
template <typename Book>
bool offer(File<Book>& file, std::shared_ptr<const Book> book)
{
    if (!book || !book->size() || book->generated() < file.book->generated()) return false;
    file.book = std::move(book);
    return true;
}

// On a worker: the file shipped with PlayShelf and the cached one, offered.
template <typename Book>
void load(File<Book>& file)
{
    std::shared_ptr<const Book> shipped = read<Book>(std::string(AURORE_ASSET("data/")) + file.name);
    std::shared_ptr<const Book> cached = read<Book>(file.cached());
    aurore::sync([&file, shipped, cached]() {
        const bool a = offer(file, shipped);
        const bool b = offer(file, cached);
        if (a || b) tell();
    });
}

// On a worker: the newest of a file, kept when it reads; false when it did not come.
// One the index says is unchanged is not downloaded again.
template <typename Book>
bool fetch(File<Book>& file)
{
    if (insights_index::unchanged(file.published(), file.cached())) return true;
    http::Response r;
    std::string    why;
    if (!http::request(std::string(BASE) + file.published(), {}, "", &r, &why, MAX_BYTES, 30) || r.status != 200) {
        aurore::Logger::debug("insights data: {} not fetched ({} {})", file.published(), r.status, why);
        return false;
    }
    std::shared_ptr<const Book> book = parse<Book>(r.body);
    if (!book) {
        aurore::Logger::warning("insights data: the fetched {} is not one", file.published());
        return false;
    }
    if (paths::ensure_dir(paths::cache_dir())) paths::atomic_write(file.cached(), r.body);
    aurore::Logger::info("insights data: {}, {} games, made {}", file.published(), book->size(), book->generated());
    aurore::sync([&file, book]() {
        if (offer(file, book)) tell();
    });
    return true;
}

// Every file fetched again, one after the other, then the fetch over.
void fetch_all()
{
    fetch(s_popular);
    fetch(s_signals);
    fetch(s_similar);
    fetch(s_editions);
    aurore::sync([]() { s_fetching = false; });
}
}   // namespace

void start()
{
    const bool online = config::get().popular_online;
    s_fetching = online;
    ui::in_background("insights data", [online]() {
        load(s_popular);
        load(s_signals);
        load(s_similar);
        load(s_editions);
        long oldest = -1;
        // The oldest cached file decides; a missing one is as old as can be.
        for (const std::string& file : { s_popular.cached(), s_signals.cached(), s_similar.cached() }) {
            const long age = insights_index::age(file);
            oldest = std::max(oldest, age < 0 ? KEEP + 1 : age);
        }
        if (online && oldest > KEEP) fetch_all();
        else aurore::sync([]() { s_fetching = false; });
    });
}

void refresh()
{
    if (s_fetching || !config::get().popular_online) return;
    s_fetching = true;
    ui::in_background("insights data", []() {
        insights_index::forget();
        fetch_all();
    });
}

library::Renown of(uint64_t base_id)
{
    return s_popular.book->of(base_id);
}

size_t size()
{
    return s_popular.book->size();
}

std::shared_ptr<const library::SignalBook> signals()
{
    return s_signals.book;
}

std::shared_ptr<const library::SimilarBook> similar()
{
    return s_similar.book;
}

std::shared_ptr<const library::EditionBook> editions()
{
    return s_editions.book;
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
