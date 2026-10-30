// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/wishlist.hpp"

#include <algorithm>
#include <memory>

#include <aurore.hpp>

#include "library/catalog.hpp"
#include "library/seen.hpp"
#include "ui/ui.hpp"
#include "util/listeners.hpp"
#include "util/paths.hpp"

namespace wishlist
{

namespace
{
std::vector<uint64_t>                s_games;   // the latest first
Listeners                            s_listeners;

std::string file()
{
    return paths::data_dir() + "/wishlist.txt";
}

void save()
{
    ui::save_later("wishlist", file(), library::ids_text(s_games));
}
}   // namespace

void start()
{
    std::string text;
    if (paths::read_file(file(), text)) s_games = library::ids_of(text, MAX_GAMES);
}

bool has(uint64_t title_id)
{
    const uint64_t base = library::base_of(title_id);
    return std::ranges::contains(s_games, base);
}

void toggle(uint64_t title_id, const std::string& name)
{
    using namespace aurore::literals;
    const uint64_t base = library::base_of(title_id);
    if (!base) return;
    const auto it = std::find(s_games.begin(), s_games.end(), base);
    if (it != s_games.end()) {
        s_games.erase(it);
        ui::notify(aurore::getStr("playshelf/wishlist/removed", name));
    } else {
        if (s_games.size() >= MAX_GAMES) {
            ui::notify("playshelf/wishlist/full"_i18n, ui::Toast::Refused);
            return;
        }
        s_games.insert(s_games.begin(), base);
        ui::notify(aurore::getStr("playshelf/wishlist/added", name), ui::Toast::Done);
    }
    save();
    s_listeners.notify();
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

}   // namespace wishlist
