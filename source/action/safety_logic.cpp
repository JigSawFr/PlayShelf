// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/safety_logic.hpp"

namespace safety_logic
{

bool risky(const Probe& probe)
{
    return probe.emummc == Answer::No || probe.nintendo_blocked == Answer::No;
}

bool blocked_address(const Address& a)
{
    if (a[0] == 0 || a[0] == 127) return true;
    return (a[0] == 95 && a[1] == 216 && a[2] == 149 && a[3] == 205) || (a[0] == 163 && a[1] == 172 && a[2] == 141 && a[3] == 219) ||
           (a[0] == 207 && a[1] == 246 && a[2] == 121 && a[3] == 77);
}

Answer nintendo_blocked(const std::vector<std::vector<Address>>& answers)
{
    bool answered = false;
    for (const std::vector<Address>& addresses : answers) {
        if (addresses.empty()) continue;
        answered = true;
        for (const Address& a : addresses)
            if (!blocked_address(a)) return Answer::No;
    }
    return answered ? Answer::Yes : Answer::Unknown;
}

}   // namespace safety_logic
