// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <stdexcept>
#include <vector>

#include "check.h"
#include "core/boost.h"
#include "util/cpu_boost.hpp"

static std::vector<bool> calls;

extern "C" void core_cpu_boost(bool on) { calls.push_back(on); }

int main()
{
    // The first holder raises it, the last lets it go; the ones between do nothing.
    cpu_boost::acquire();
    cpu_boost::acquire();
    CHECK(calls == std::vector<bool>({ true }) && cpu_boost::holders() == 2);
    cpu_boost::release();
    CHECK(calls.size() == 1 && cpu_boost::holders() == 1);
    cpu_boost::release();
    CHECK(calls == std::vector<bool>({ true, false }) && cpu_boost::holders() == 0);

    // A release with nobody holding it is ignored.
    cpu_boost::release();
    CHECK(calls.size() == 2 && cpu_boost::holders() == 0);

    // A guard holds it for its scope, nested ones included, and lets go on an exception.
    calls.clear();
    {
        cpu_boost::Guard outer(true);
        {
            cpu_boost::Guard inner(true);
            CHECK(cpu_boost::holders() == 2);
        }
        CHECK(cpu_boost::holders() == 1 && calls.size() == 1);
    }
    CHECK(calls == std::vector<bool>({ true, false }));
    calls.clear();
    try {
        cpu_boost::Guard g(true);
        throw std::runtime_error("install failed");
    } catch (const std::runtime_error&) {
    }
    CHECK(calls == std::vector<bool>({ true, false }) && cpu_boost::holders() == 0);

    // Disabled in the settings: it holds nothing.
    calls.clear();
    { cpu_boost::Guard off(false); CHECK(cpu_boost::holders() == 0); }
    CHECK(calls.empty());

    // The app's exit puts it back whoever holds it; their own releases then do nothing.
    cpu_boost::acquire();
    cpu_boost::acquire();
    calls.clear();
    cpu_boost::release_all();
    CHECK(calls == std::vector<bool>({ false }) && cpu_boost::holders() == 0);
    cpu_boost::release();
    CHECK(calls.size() == 1);
    CHECK_DONE("cpu_boost");
}
