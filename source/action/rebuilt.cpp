// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/rebuilt.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <aurore.hpp>

#include "library/seen.hpp"
#include "library/titles.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

namespace rebuilt
{

namespace
{
constexpr size_t MAX_GAMES = 2000;

std::set<uint64_t> s_games;
bool               s_newer = false;   // the file is a newer build's: never written over

std::string file()
{
    return paths::data_dir() + "/rebuilt.txt";
}

void keep()
{
    library::set_rebuilt(s_games);
    if (s_newer) return;
    ui::save_later("rebuilt", file(), library::ids_text(std::vector<uint64_t>(s_games.begin(), s_games.end())));
}
}   // namespace

void start()
{
    std::string text;
    if (!paths::read_file(file(), text)) return;
    s_newer = library::ids_newer(text);
    if (s_newer) aurore::Logger::warning("rebuilt: {} is from a newer PlayShelf, left as it is", file());
    const std::vector<uint64_t> ids = library::ids_of(text, MAX_GAMES);
    s_games.insert(ids.begin(), ids.end());
    library::set_rebuilt(s_games);
}

void note(const std::map<uint64_t, bool>& programs)
{
    bool changed = false;
    for (const auto& [base, is_rebuilt] : programs) {
        if (is_rebuilt) changed |= s_games.size() < MAX_GAMES && s_games.insert(base).second;
        else changed |= s_games.erase(base) > 0;
    }
    if (changed) keep();
}

}   // namespace rebuilt
