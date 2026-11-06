// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/crypto_flow.hpp"

#include <aurore.hpp>
#include <aurore/cadran/ui/ui.hpp>

#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/crypto_backend.hpp"
#include "util/crypto_bench.hpp"

namespace crypto_flow
{
namespace
{
constexpr const char* NAMES[] = { "auto", "software", "hardware" };
}   // namespace

void apply()
{
    static bool offered = false;
    if (!offered) {
        offered = true;
        crypto::offer_engine(crypto::hardware_engine());
    }
    crypto::set_backend(crypto::backend_from_name(config::get().crypto));
    aurore::Logger::info("{}", crypto::status());
}

std::string text()
{
    const crypto::Backend b = crypto::backend_from_name(config::get().crypto);
    std::string           out = b == crypto::Backend::Software ? "Software" : b == crypto::Backend::Hardware ? "Hardware" : "Automatic";
    out += crypto::aes_engine() ? " (hardware in use)" : " (software in use)";
    return out;
}

void choose(std::function<void()> done)
{
    const std::vector<std::string> labels = { "Automatic: hardware when it checks out", "Software", "Hardware" };
    int                            now = 0;
    for (int i = 0; i < 3; i++)
        if (config::get().crypto == NAMES[i]) now = i;
    ui::pick("Encryption", labels, now, [done](int i) {
        config::get().crypto = NAMES[i];
        ui::save_config();
        apply();
        if (done) done();
    });
}

void benchmark()
{
    ui::notify("Measuring… a few seconds");
    ui::in_background("crypto benchmark", []() {
        const std::string text = crypto::bench_text(crypto::benchmark(std::chrono::milliseconds(1000)));
        aurore::Logger::info("benchmark: {}", text);
        aurore::sync([text]() { ui::confirm(text, "Close", []() {}); });
    });
}
}   // namespace crypto_flow
