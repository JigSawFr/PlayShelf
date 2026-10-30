// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/no_prices.hpp"

#include <filesystem>
#include <system_error>
#include <vector>

#include <aurore/extern/nlohmann/json.hpp>

#include "action/notice_logic.hpp"
#include "util/paths.hpp"

namespace no_prices
{

size_t clean(const std::string& config_file, int config_schema, const std::string& cache_dir,
             const std::string& read_keys_file)
{
    size_t gone = 0;

    // The cached prices of each country.
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(cache_dir, ec)) {
        const std::string name = e.path().filename().string();
        if (name.starts_with("prices_") && name.ends_with(".json") && std::filesystem::remove(e.path(), ec)) gone++;
    }

    // config.json's prices_country, its other keys as they are.
    std::string text;
    if (paths::read_file(config_file, text)) {
        nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
        const auto     schema = j.is_object() ? j.find("schema") : j.end();
        const bool     newer = schema != j.end() && schema->is_number_unsigned() && schema->get<uint64_t>() > (uint64_t)config_schema;
        if (j.is_object() && !newer && j.erase("prices_country") &&
            paths::atomic_write(config_file, j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n"))
            gone++;
    }

    // The bell's sale and lowest-price entries put away.
    text.clear();
    if (paths::read_file(read_keys_file, text) && !notice_logic::keys_newer(text)) {
        std::vector<std::string> keys = notice_logic::keys_of(text);
        const size_t             n = std::erase_if(keys, [](const std::string& k) { return k.starts_with("sale.") || k.starts_with("lowest."); });
        if (n && paths::atomic_write(read_keys_file, notice_logic::keys_text(keys))) gone += n;
    }
    return gone;
}

}   // namespace no_prices
