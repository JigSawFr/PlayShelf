// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/space_logic.hpp"

#include <algorithm>
#include <numeric>

namespace space_logic
{

uint64_t shortfall(uint64_t free, uint64_t remaining)
{
    const uint64_t need = remaining + SPARE;
    if (need < remaining) return UINT64_MAX;   // wrapped around
    return need > free ? need - free : 0;
}

Plan plan(uint64_t missing, const std::vector<Candidate>& games)
{
    Plan out;
    if (missing == 0) {
        out.enough = true;
        return out;
    }
    std::vector<size_t> order;
    for (size_t i = 0; i < games.size(); i++)
        if (!games[i].keep && games[i].size > 0) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const Candidate &x = games[a], &y = games[b];
        const bool      nx = x.last_played == 0, ny = y.last_played == 0;
        if (nx != ny) return !nx;   // played ones first
        if (!nx && x.last_played != y.last_played) return x.last_played < y.last_played;
        return x.size > y.size;
    });
    uint64_t freed = 0;
    size_t   taken = 0;
    while (taken < order.size() && freed < missing) freed += games[order[taken++]].size;
    out.enough = freed >= missing;
    if (!out.enough) {
        out.freed = freed;
        out.picks.assign(order.begin(), order.end());
        return out;
    }
    order.resize(taken);
    // Needless ones dropped, the most recently played first: what the others free is enough without them.
    for (size_t k = order.size(); k-- > 0;) {
        if (freed - games[order[k]].size >= missing) {
            freed -= games[order[k]].size;
            order.erase(order.begin() + (long)k);
        }
    }
    out.freed = freed;
    out.picks = std::move(order);
    return out;
}

}   // namespace space_logic
