// no_prices — migrations step 1 (source/main.cpp): PlayShelf no longer
// shows eShop prices or sales, so what they left in the data folder goes:
// the cached prices (cache_dir()/prices_<COUNTRY>.json), config.json's
// `prices_country` (written back by every build since, as a key it does not
// know), and the bell's read keys of the sale and lowest-price entries
// (notices_read.txt: `sale.…`, `lowest.…`). A file of a newer schema is left
// as it is. This step, and this file, go once every build that had prices is
// past: with the first release after the one that brings it (plans:
// playshelf/suivi.md). Plain C++ for the host tests (tests/no_prices).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <string>

namespace no_prices
{

// What the prices left, removed; how many entries went (files, keys).
// `config_schema`: config.json's (config::SCHEMA).
size_t clean(const std::string& config_file, int config_schema, const std::string& cache_dir,
             const std::string& read_keys_file);

}   // namespace no_prices
