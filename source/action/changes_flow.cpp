// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/changes_flow.hpp"

#include <aurore.hpp>

#include "action/insights_index.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/http.hpp"
#include "util/listeners.hpp"
#include "util/paths.hpp"

namespace changes_flow
{

namespace
{
constexpr const char* NAME = "changes.json";   // at the root of Atlas' data branch
constexpr const char* BASE = "https://cdn.jsdelivr.net/gh/JigSawFr/Atlas@data/";
constexpr size_t      MAX_BYTES = 1024 * 1024;
constexpr long        KEEP = 24 * 3600;       // fetched again a day after
constexpr long        STALE = 2 * 24 * 3600;  // a cached file older is not shown: old news

std::shared_ptr<const library::ChangeBook> s_book = std::make_shared<library::ChangeBook>();
int                                        s_run = 0;   // the latest start(): an older one's answer is dropped
Listeners                                  s_listeners;

std::string cached()
{
    return paths::cache_dir() + "/changes.json";
}

std::shared_ptr<const library::ChangeBook> parse(const std::string& text)
{
    auto book = std::make_shared<library::ChangeBook>();
    return book->from_json(text) ? book : nullptr;
}

// On the UI thread: `book` shown, when it is the latest start's.
void offer(int run, std::shared_ptr<const library::ChangeBook> book)
{
    if (run != s_run || !book) return;
    s_book = std::move(book);
    s_listeners.notify();
}
}   // namespace

void start()
{
    const int  run = ++s_run;
    const bool online = config::get().popular_online;
    ui::in_background("changes", [run, online]() {
        const std::string file = cached();
        const long        age = insights_index::age(file);
        std::string       text;
        const bool        shown = age >= 0 && age < STALE && paths::read_file(file, text);
        if (shown) {
            auto book = parse(text);
            aurore::sync([run, book]() { offer(run, book); });
        }
        if (!online || (age >= 0 && age < KEEP)) return;
        // Unchanged since (the index says): kept, and shown when it was too old to be.
        if (insights_index::unchanged(NAME, file)) {
            if (!shown && paths::read_file(file, text)) {
                auto book = parse(text);
                aurore::sync([run, book]() { offer(run, book); });
            }
            return;
        }
        http::Response r;
        std::string    why;
        if (!http::request(std::string(BASE) + NAME, {}, "", &r, &why, MAX_BYTES, 30) || r.status != 200) {
            aurore::Logger::debug("changes: not fetched ({} {})", r.status, why);
            return;
        }
        auto book = parse(r.body);
        if (!book) {
            aurore::Logger::warning("changes: the fetched file is not one of schema 1, not used");
            return;
        }
        ui::keep_copy("changes", file, r.body);
        aurore::Logger::info("changes: made {}, {} games", book->generated(), book->size());
        aurore::sync([run, book]() { offer(run, book); });
    });
}

std::shared_ptr<const library::ChangeBook> book()
{
    return s_book;
}

int listen(std::function<void()> changed)
{
    return s_listeners.listen(std::move(changed));
}

void unlisten(int id)
{
    s_listeners.unlisten(id);
}

}   // namespace changes_flow
