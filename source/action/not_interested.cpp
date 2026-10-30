// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/not_interested.hpp"

#include <algorithm>
#include <memory>

#include <aurore.hpp>

#include "library/catalog.hpp"
#include "library/seen.hpp"
#include "ui/ui.hpp"
#include "util/listeners.hpp"
#include "util/paths.hpp"

namespace not_interested
{

namespace
{
std::vector<uint64_t>                s_games;   // the latest first
Listeners                            s_listeners;

std::string file()
{
    return paths::data_dir() + "/not_interested.txt";
}

void keep()
{
    ui::save_later("not_interested", file(), library::ids_text(s_games));
    s_listeners.notify();
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
    return s_listeners.listen(std::move(changed));
}

void unlisten(int id)
{
    s_listeners.unlisten(id);
}

}   // namespace not_interested
