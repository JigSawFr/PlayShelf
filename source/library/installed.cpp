// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/installed.hpp"

#include <algorithm>

namespace library
{

std::vector<InstalledGame> installed_games(const std::map<uint64_t, uint32_t>& installed)
{
    std::map<uint64_t, InstalledGame> by_game;
    for (const auto& [id, version] : installed) {
        if (!id) continue;
        const uint64_t base = base_of(id);
        InstalledGame& g = by_game[base];
        g.id = base;
        switch (kind_of(id)) {
        case Kind::Update: g.update = std::max(g.update, version); break;
        case Kind::Dlc: g.dlcs.push_back(id); break;
        default:
            g.base = true;
            g.version = version;
            break;
        }
    }
    std::vector<InstalledGame> out;
    out.reserve(by_game.size());
    for (auto& kv : by_game) out.push_back(std::move(kv.second));
    return out;
}

long title_of(const std::vector<Title>& titles, uint64_t id)
{
    for (size_t i = 0; i < titles.size(); i++)
        if (titles[i].id == id) return (long)i;
    return -1;
}

}   // namespace library
