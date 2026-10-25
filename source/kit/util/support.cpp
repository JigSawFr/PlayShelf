// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/support.hpp"

#include "util/date.hpp"

namespace support
{

Decision decide(const State& state, const std::string& version, const std::string& today, bool has_notes)
{
    Decision d;
    d.next = state;
    if (state.seen_version != version) {
        if (!state.seen_version.empty() && has_notes) d.show = Show::WhatsNew;
        d.next.seen_version = version;
        d.next.reminded     = today;
        return d;
    }
    int days = 0;
    if (!date::days_between(state.reminded, today, &days) || days < 0) {
        d.next.reminded = today;   // not counting yet, or the clock went back
        return d;
    }
    if (state.reminder_on && days >= REMINDER_DAYS) {
        d.show          = Show::Reminder;
        d.next.reminded = today;
    }
    return d;
}

}   // namespace support
