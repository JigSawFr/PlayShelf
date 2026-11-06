// crypto_flow — the encryption setting of Tools › Developer (config crypto:
// auto, software, hardware): it puts the setting in force (util/
// crypto_backend), offering the CPU's engine to be checked the first time,
// lets the developer choose it, and runs the benchmark (util/crypto_bench).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>

namespace crypto_flow
{
// Puts config crypto in force and logs what runs. At start-up and after a change.
void apply();
// The setting and what it comes to, for the cell's detail line.
std::string text();
// Asks which one; `done` once chosen.
void choose(std::function<void()> done);
// Measures in the background, then shows the results.
void benchmark();
}   // namespace crypto_flow
