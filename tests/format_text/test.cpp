// Host tests for source/kit/ui/format_text.cpp, translated by the real
// catalogs (resources/i18n/<lang>/playshelf.json, read from the repository
// root `make test` runs in) through ui/tr.hpp's stand-in for borealis:
// minutes ("45 min", "2 h", "2 h 05"), a duration in nanoseconds (rounded to
// the minute) and in seconds over any length ("< 1 min", "52 h 30", "278 h"),
// the day names (in a title and inside a sentence, French for the
// difference), yes / no, "Unavailable", and the chevron of a line that opens
// a screen. Every key the helpers ask for exists in the catalog.
#include "check.h"
#include <cstdint>
#include <fstream>
#include <map>
#include <borealis/extern/nlohmann/json.hpp>
#include <set>
#include <string>

#include "ui/format_text.hpp"
#include "ui/tr.hpp"

static std::map<std::string, std::string> s_catalog;
static std::set<std::string>              s_missing;   // keys asked for that the catalog lacks

static void flatten(const nlohmann::json& j, const std::string& prefix)
{
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it->is_object())
            flatten(*it, prefix + it.key() + "/");
        else if (it->is_string())
            s_catalog[prefix + it.key()] = it->get<std::string>();
    }
}

static bool load(const std::string& lang)
{
    s_catalog.clear();
    std::ifstream in("resources/i18n/" + lang + "/playshelf.json");
    if (!in) return false;
    flatten(nlohmann::json::parse(in), "playshelf/");
    return !s_catalog.empty();
}

namespace brls
{
namespace internal
{
// As borealis: a key the catalog lacks reads as itself.
std::string getRawStr(std::string name)
{
    const auto it = s_catalog.find(name);
    if (it != s_catalog.end()) return it->second;
    s_missing.insert(name);
    return name;
}
}   // namespace internal

inline namespace literals
{
std::string operator""_i18n(const char* str, size_t len)
{
    return internal::getRawStr(std::string(str, len));
}
}   // namespace literals
}   // namespace brls

static void test_minutes()
{
    CHECK(ui::fmt_minutes(0) == "0\u00A0min");
    CHECK(ui::fmt_minutes(1) == "1\u00A0min" && ui::fmt_minutes(59) == "59\u00A0min");
    CHECK(ui::fmt_minutes(60) == "1\u00A0h" && ui::fmt_minutes(120) == "2\u00A0h" && ui::fmt_minutes(1440) == "24\u00A0h");
    CHECK(ui::fmt_minutes(125) == "2\u00A0h\u00A005" && ui::fmt_minutes(150) == "2\u00A0h\u00A030" && ui::fmt_minutes(61) == "1\u00A0h\u00A001");

    const uint64_t MIN = 60ULL * 1000000000ULL;
    CHECK(ui::fmt_duration_ns(0) == "0\u00A0min");
    CHECK(ui::fmt_duration_ns(29 * 1000000000ULL) == "0\u00A0min");
    CHECK(ui::fmt_duration_ns(30 * 1000000000ULL) == "1\u00A0min");
    CHECK(ui::fmt_duration_ns(90 * MIN + 29 * 1000000000ULL) == "1\u00A0h\u00A030");
    CHECK(ui::fmt_duration_ns(90 * MIN + 30 * 1000000000ULL) == "1\u00A0h\u00A031");
    CHECK(ui::fmt_duration_ns(5000 * MIN) == "83\u00A0h\u00A020");

    CHECK(ui::fmt_duration(0) == "0\u00A0min");
    CHECK(ui::fmt_duration(1) == "< 1 min" && ui::fmt_duration(59) == "< 1 min");
    CHECK(ui::fmt_duration(60) == "1\u00A0min" && ui::fmt_duration(89) == "1\u00A0min" && ui::fmt_duration(90) == "2\u00A0min");
    CHECK(ui::fmt_duration(3600) == "1\u00A0h" && ui::fmt_duration(3600 + 5 * 60) == "1\u00A0h\u00A005");
    CHECK(ui::fmt_duration(99 * 3600 + 59 * 60) == "99\u00A0h\u00A059");
    // From 100 h, whole hours, rounded.
    CHECK(ui::fmt_duration(152 * 3600 + 29 * 60) == "152\u00A0h" && ui::fmt_duration(152 * 3600 + 30 * 60) == "153\u00A0h");
    CHECK(ui::fmt_duration(277 * 3600 + 58 * 60) == "278\u00A0h");
    CHECK(ui::fmt_duration(100000ULL * 3600) == "100000\u00A0h");
}

static void test_names()
{
    CHECK(ui::day_name(0) == "Sunday" && ui::day_name(1) == "Monday" && ui::day_name(6) == "Saturday");
    CHECK(ui::day_name(-1) == "?" && ui::day_name(7) == "?");
    CHECK(ui::day_name_in_text(3) == "Wednesday" && ui::day_name_in_text(7) == "?" && ui::day_name_in_text(-1) == "?");

    CHECK(ui::yes_no(true) == "Yes" && ui::yes_no(false) == "No");
    CHECK(ui::bool_text(true, true, "on", "off") == "on" && ui::bool_text(true, false, "on", "off") == "off");
    CHECK(ui::bool_text(false, true, "on", "off") == "Unavailable");

    CHECK(ui::opens() == "›" && ui::opens("") == "›" && ui::opens("12 changes") == "12 changes  ›");
}

// Another language: its own words, its own cases ("lundi" inside a sentence).
static void test_french()
{
    REQUIRE(load("fr"));
    CHECK(ui::day_name(1) == s_catalog["playshelf/days/1"]);
    CHECK(ui::day_name_in_text(1) == "lundi");
    CHECK(ui::fmt_minutes(125) == "2\u00A0h\u00A005");
    CHECK(ui::fmt_duration(30) == s_catalog["playshelf/common/less_than_minute"]);
}

int main()
{
    REQUIRE(load("en-US"));
    test_minutes();
    test_names();
    test_french();
    for (const std::string& k : s_missing) std::fprintf(stderr, "no such key: %s\n", k.c_str());
    CHECK(s_missing.empty());
    return CHECK_DONE("format_text tests passed");
}
