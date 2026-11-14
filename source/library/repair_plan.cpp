// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/repair_plan.hpp"

#include <algorithm>

namespace library
{

namespace
{
long item_for(const Title& t, const std::vector<Item>& items, const Damaged& d)
{
    switch (d.kind) {
        case Kind::Base:
            return t.base >= 0 && (size_t)t.base < items.size() ? t.base : -1;
        case Kind::Update:
            for (const size_t i : t.updates)
                if (i < items.size() && items[i].title_id == d.id && items[i].has_version && items[i].version == d.version) return (long)i;
            return -1;
        case Kind::Dlc:
            for (const size_t i : t.dlcs)
                if (i < items.size() && items[i].title_id == d.id && (!items[i].has_version || items[i].version == d.version)) return (long)i;
            return -1;
        default: return -1;
    }
}
}   // namespace

RepairPlan plan_repair(const Title& title, const std::vector<Item>& items, const std::vector<Damaged>& damaged)
{
    RepairPlan plan;
    for (const Damaged& d : damaged) {
        const long i = item_for(title, items, d);
        if (i < 0) {
            plan.missing.push_back(d);
            continue;
        }
        if (std::ranges::contains(plan.items, (size_t)i)) continue;
        plan.items.push_back((size_t)i);
        plan.bytes += items[(size_t)i].size;
    }
    return plan;
}

}   // namespace library
