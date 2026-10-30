// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/not_interested.hpp"

#include <algorithm>
#include <map>
#include <memory>

#include <aurore.hpp>

#include "library/catalog.hpp"
#include "library/seen.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

namespace not_interested
{

namespace
{
std::vector<uint64_t>                s_games;   // the latest first
std::map<int, std::function<void()>> s_listeners;
int                                  s_next = 1;

std::string file()
{
    return paths::data_dir() + "/not_interested.txt";
}

void keep()
{
    auto text = std::make_shared<std::string>(library::ids_text(s_games));
    ui::in_background("not_interested", [text]() {
        // Saved again at the next change: a failed write is not reported.
        if (paths::ensure_dir(paths::data_dir())) (void)paths::atomic_write(file(), *text);
    });
    const auto listeners = s_listeners;
    for (const auto& l : listeners) l.second();
}
}   // namespace

void start()
{
    std::string text;
    if (paths::read_file(file(), text)) s_games = library::ids_of(text, MAX_GAMES);
}

void add(uint64_t title_id, const std::string& name)
{
    const uint64_t base = library::base_of(title_id);
    if (!base || std::ranges::contains(s_games, base)) return;
    s_games.insert(s_games.begin(), base);
    // The oldest go when it is full: what was put away long ago may please now.
    if (s_games.size() > MAX_GAMES) s_games.resize(MAX_GAMES);
    ui::notify(aurore::getStr("playshelf/not_interested/added", name), ui::Toast::Done);
    keep();
}

void clear()
{
    if (s_games.empty()) return;
    s_games.clear();
    ui::notify(aurore::getStr("playshelf/not_interested/cleared"), ui::Toast::Done);
    keep();
}

std::vector<uint64_t> games()
{
    return s_games;
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

}   // namespace not_interested
