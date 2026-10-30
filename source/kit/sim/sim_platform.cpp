// Desktop simulation of core/platform.h: PLAYSHELF_SIM_* variables answer
// for the console (listed in platform.h); what would reach the console is
// printed instead, so the smoke test can read it from the log.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/platform.h"

#include "core/sim_env.h"
#include "util/sha256.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <random>

bool platform_in_focus(void)
{
    return !sim_env_flag("PLAYSHELF_SIM_BACKGROUND");
}

bool platform_game_open(bool* known)
{
    if (known) *known = true;
    return !sim_env_flag("PLAYSHELF_SIM_NO_GAME");
}

bool platform_can_launch(void)
{
    return sim_env_flag("PLAYSHELF_SIM_HBLOADER");
}

bool platform_set_next_load(const char* path)
{
    return platform_set_next_load_args(path, path);
}

bool platform_set_next_load_args(const char* path, const char* args)
{
    if (!path || !args || !platform_can_launch()) return false;
    std::printf("next load: %s\n", args);
    std::fflush(stdout);
    return true;
}

void platform_exit_to_home(void)
{
    if (sim_env_flag("PLAYSHELF_SIM_APPLET")) return;
    std::printf("exit to HOME\n");
    std::fflush(stdout);
}

bool platform_boot_choice(const char* text)
{
    const char* choice = sim_env("PLAYSHELF_SIM_BOOT_CHOICE");
    const bool a = choice && std::strcmp(choice, "a") == 0;
    std::printf("boot screen:\n%s\nboot screen: %s\n", text ? text : "", a ? "A" : "B");
    std::fflush(stdout);
    return a;
}

void platform_romfs_release(void) {}
void platform_romfs_mount(void) {}

bool platform_region(int* region)
{
    const long long r = sim_env_int("PLAYSHELF_SIM_REGION", -1, 0, 255);
    if (!region || r < 0) return false;
    *region = (int)r;
    return true;
}

bool platform_random(void* buf, size_t size)
{
    if (!buf) return false;
    try {
        std::random_device random;
        auto* bytes = static_cast<unsigned char*>(buf);
        for (size_t i = 0; i < size; i++) bytes[i] = static_cast<unsigned char>(random());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// The simulator's addresses mean nothing to a crash file.
bool platform_code_range(uint64_t*, uint64_t*) { return false; }
bool platform_read64(uint64_t, uint64_t*, void*) { return false; }
bool platform_memory(uint64_t*, uint64_t*) { return false; }

void platform_crash(void)
{
    std::abort();
}
