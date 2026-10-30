// util/no_prices: what the eShop prices left in the data folder removed
// (the cached prices, config.json's prices_country, the read keys of the
// sale and lowest-price entries), the rest kept; a file of a newer schema
// left as it is; nothing to do the second time.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <filesystem>
#include <string>

#include "action/notice_logic.hpp"
#include "check.h"
#include "util/no_prices.hpp"
#include "util/paths.hpp"

namespace
{
namespace fs = std::filesystem;

const fs::path ROOT = fs::temp_directory_path() / "playshelf_no_prices";

void put(const fs::path& p, const std::string& text) { CHECK(paths::atomic_write(p.string(), text)); }

std::string get(const fs::path& p)
{
    std::string text;
    paths::read_file(p.string(), text);
    return text;
}

void test_clean()
{
    fs::remove_all(ROOT);
    fs::create_directories(ROOT / "cache");
    put(ROOT / "cache/prices_FR.json", "{}");
    put(ROOT / "cache/prices_JP.json", "{}");
    put(ROOT / "cache/changes.json", "{}");
    put(ROOT / "config.json", R"({"schema": 1, "theme": "dark", "prices_country": "FR", "later": [1]})");
    const std::string config = (ROOT / "config.json").string(), keys = (ROOT / "notices_read.txt").string();
    put(keys, notice_logic::keys_text({ "sale.0100000000010000.20261112:0123abcd", "wish.0100000000010000:0123abcd",
                                        "lowest.0100000000020000.20261102:89abcdef" }));
    CHECK(no_prices::clean(config, 1, (ROOT / "cache").string(), keys) == 5);
    CHECK(!fs::exists(ROOT / "cache/prices_FR.json") && !fs::exists(ROOT / "cache/prices_JP.json"));
    CHECK(fs::exists(ROOT / "cache/changes.json"));
    const std::string after = get(ROOT / "config.json");
    CHECK(after.find("prices_country") == std::string::npos && after.find("\"theme\": \"dark\"") != std::string::npos &&
          after.find("\"later\"") != std::string::npos);
    const auto left = notice_logic::keys_of(get(keys));
    CHECK(left.size() == 1 && left[0] == "wish.0100000000010000:0123abcd");
    // Again: nothing left to do, nothing written.
    CHECK(no_prices::clean(config, 1, (ROOT / "cache").string(), keys) == 0);
}

void test_newer_and_missing()
{
    fs::remove_all(ROOT);
    fs::create_directories(ROOT);
    // A newer config.json, a newer read keys file: left as they are.
    const std::string config = (ROOT / "config.json").string(), keys = (ROOT / "notices_read.txt").string();
    const std::string newer = R"({"schema": 2, "prices_country": "FR"})";
    put(ROOT / "config.json", newer);
    put(keys, "schema=2\nsale.01:0123abcd\n");
    CHECK(no_prices::clean(config, 1, (ROOT / "cache").string(), keys) == 0);
    CHECK(get(ROOT / "config.json") == newer && get(keys) == "schema=2\nsale.01:0123abcd\n");
    // Nothing there at all, or not JSON: nothing done.
    fs::remove_all(ROOT);
    CHECK(no_prices::clean(config, 1, (ROOT / "cache").string(), keys) == 0);
    fs::create_directories(ROOT);
    put(ROOT / "config.json", "not json");
    CHECK(no_prices::clean(config, 1, (ROOT / "cache").string(), keys) == 0 && get(ROOT / "config.json") == "not json");
    fs::remove_all(ROOT);
}
}   // namespace

int main()
{
    test_clean();
    test_newer_and_missing();
    CHECK_DONE("no_prices");
}
