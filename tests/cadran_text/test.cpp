// Host tests for source/ui/cadran_text.cpp, translated by the real catalogs
// (Aurore's extern/borealis/resources/i18n/<lang>/cadran.json, read from the repository root `make
// test` runs in) through ui/tr.hpp's stand-in for borealis: a dialog's table
// marked and read back, a week's identical lines as one (and a short table
// left alone), and a failure's title, reason, code and report, in English and
// French, with or without each part.
#include "check.h"
#include <fstream>
#include <map>
#include <borealis/extern/nlohmann/json.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <borealis/cadran/ui/cadran_text.hpp>
#include <borealis/cadran/ui/tr.hpp>

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
    std::ifstream in("extern/borealis/resources/i18n/" + lang + "/cadran.json");
    if (!in) return false;
    flatten(nlohmann::json::parse(in), "cadran/");
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

using Rows = std::vector<std::pair<std::string, std::string>>;

static void test_table()
{
    const Rows rows = { { "When", "Monday 14:02" }, { "Before", "1 h" }, { "After", "" }, { "Ver\u00A0\u00B7 x", "1.2" } };
    Rows back;
    CHECK(cadran_text::table_rows(cadran_text::table(rows), &back) && back == rows);
    // Words, even with a tab or "a: b" in them, are no table.
    CHECK(!cadran_text::table_rows("When\tMonday", &back));
    CHECK(!cadran_text::table_rows("When: Monday", &back));
    CHECK(!cadran_text::table_rows("", &back));
    CHECK(!cadran_text::table_rows(cadran_text::table({}), &back));
}

static void test_group_rows()
{
    using cadran_text::group_rows;
    const Rows week = { { "Monday", "2 h" }, { "Tuesday", "2 h" }, { "Wednesday", "2 h" }, { "Thursday", "2 h" },
                        { "Friday", "2 h" }, { "Saturday", "3 h" }, { "Sunday", "3 h" } };
    const Rows grouped = group_rows(week);
    CHECK(grouped.size() == 2);
    CHECK(grouped[0] == std::make_pair(std::string("Monday \u2013 Friday"), std::string("2 h")));
    CHECK(grouped[1] == std::make_pair(std::string("Saturday \u2013 Sunday"), std::string("3 h")));
    // Every day different: as they are; a value back again later: not joined.
    const Rows mixed = { { "a", "1" }, { "b", "2" }, { "c", "1" }, { "d", "2" }, { "e", "1" } };
    CHECK(group_rows(mixed) == mixed);
    const Rows tail = { { "a", "1" }, { "b", "2" }, { "c", "2" }, { "d", "3" }, { "e", "3" } };
    CHECK(group_rows(tail).size() == 3 && group_rows(tail)[1].first == "b \u2013 c");
    // A short table (a few settings) is never grouped.
    const Rows short_table = { { "a", "1" }, { "b", "1" }, { "c", "1" }, { "d", "1" } };
    CHECK(group_rows(short_table) == short_table);
    CHECK(group_rows({}).empty());
}

static void test_failure()
{
    using cadran_text::failure;
    // The console's code: its hint as the reason, the code small, a report.
    cadran_text::Failure f = failure("Could not unlock parental controls", "refused by the service", 0xC88E, false);
    CHECK(f.title == "Could not unlock parental controls");
    CHECK(f.reason == "Refused by the service");
    CHECK(f.code == brls::getStr("cadran/error/code", "0x0000C88E"));
    CHECK(f.report);

    // No hint known: the code alone.
    f = failure("Could not save", "", 0x1234, false);
    CHECK(f.title == "Could not save" && f.reason.empty() && f.code == brls::getStr("cadran/error/code", "0x00001234"));

    // One of PlayShelf's own results: its words, no code nor report.
    f = failure("", "The SD card is full.", 0x5678, true);
    CHECK(f.title.empty() && f.reason == "The SD card is full." && f.code.empty() && !f.report);

    // A hint starting with a digit or a capital stays as it is.
    CHECK(failure("x", "3 tries left", 1, true).reason == "3 tries left");
}

// French: its own words for the code.
static void test_french()
{
    REQUIRE(load("fr"));
    const cadran_text::Failure f = cadran_text::failure("Impossible de d\u00E9verrouiller", "refus\u00E9", 0xC88E, false);
    CHECK(f.title == "Impossible de d\u00E9verrouiller" && f.reason == "Refus\u00E9" && f.report);
    CHECK(f.code == brls::getStr("cadran/error/code", "0x0000C88E"));
    CHECK(f.code.find("0x0000C88E") != std::string::npos);
}

int main()
{
    REQUIRE(load("en-US"));
    test_table();
    test_group_rows();
    test_failure();
    test_french();
    for (const std::string& k : s_missing) std::fprintf(stderr, "no such key: %s\n", k.c_str());
    CHECK(s_missing.empty());
    return CHECK_DONE("cadran_text tests passed");
}
