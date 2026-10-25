// ui — durations, days and short answers as text.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/format_text.hpp"

#include <fmt/format.h>

#include "ui/tr.hpp"

using namespace brls::literals;

namespace
{
// "2 h 30" with no-break spaces: a line never ends on "2 h" before its "30"
// (patches/borealis/0010).
std::string glued(std::string text)
{
    for (size_t at = text.find(' '); at != std::string::npos; at = text.find(' ', at + 2)) text.replace(at, 1, " ");
    return text;
}
}   // namespace

namespace ui
{

std::string fmt_minutes(uint32_t m)
{
    if (m < 60) return glued(brls::getStr("playshelf/common/minutes", (int)m));
    if (m % 60 == 0) return glued(brls::getStr("playshelf/common/hours", (unsigned long long)(m / 60)));
    return glued(brls::getStr("playshelf/common/hours_minutes", (unsigned long long)(m / 60),
                              fmt::format("{:02d}", (int)(m % 60))));
}

std::string fmt_duration_ns(uint64_t ns)
{
    return fmt_minutes((uint32_t)((ns + 30000000000ULL) / 60000000000ULL));
}

std::string fmt_duration(uint64_t seconds)
{
    if (seconds > 0 && seconds < 60) return "playshelf/common/less_than_minute"_i18n;
    const uint64_t m = (seconds + 30) / 60;
    if (m < 60) return glued(brls::getStr("playshelf/common/minutes", (int)m));
    // From 100 h, the minutes are noise: whole hours, rounded.
    if (m % 60 == 0 || m >= 100 * 60) return glued(brls::getStr("playshelf/common/hours", (unsigned long long)((m + 30) / 60)));
    return glued(brls::getStr("playshelf/common/hours_minutes", (unsigned long long)(m / 60), fmt::format("{:02d}", (int)(m % 60))));
}

std::string day_name(int day)
{
    if (day < 0 || day > 6) return "?";
    return brls::getStr(fmt::format("playshelf/days/{}", day));
}

std::string day_name_in_text(int day)
{
    if (day < 0 || day > 6) return "?";
    return brls::getStr(fmt::format("playshelf/days_lower/{}", day));
}

std::string bool_text(bool ok, bool value, const std::string& yes, const std::string& no)
{
    if (!ok) return "playshelf/common/unavailable"_i18n;
    return value ? yes : no;
}

std::string yes_no(bool value)
{
    return value ? "playshelf/common/yes"_i18n : "playshelf/common/no"_i18n;
}

std::string opens(const std::string& value)
{
    return value.empty() ? "›" : value + "  ›";
}

}   // namespace ui
