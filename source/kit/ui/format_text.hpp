// ui — durations, days and short answers as text: the formatting helpers
// that only translate (ui.hpp includes this header). Plain C++ over
// ui/tr.hpp, so a host suite can run them against a catalog of its own
// (tests/format_text).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace ui
{

std::string fmt_minutes(uint32_t minutes);      // "0 min", "45 min", "2 h", "2 h 30"
std::string fmt_duration_ns(uint64_t ns);       // rounded to minutes, as fmt_minutes
std::string fmt_duration(uint64_t seconds);     // "0 min", "< 1 min", "45 min", "52 h 30", "278 h" (no 24 h cap)
std::string day_name(int day);                  // 0 = Sunday, as a title ("Monday")
std::string day_name_in_text(int day);          // inside a sentence (fr: "lundi")
std::string bool_text(bool ok, bool value, const std::string& yes, const std::string& no);
std::string yes_no(bool value);                // "Yes" / "No"
// The value of a line that opens a screen of its own (or another tab), with
// a chevron after it: "12 changes  ›", or "›" alone. A line that asks first,
// in a dialog, ends its title with "…" instead; one that acts at once, neither.
std::string opens(const std::string& value = "");

}   // namespace ui
