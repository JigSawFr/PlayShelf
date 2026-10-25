// libFuzzer target: source/util/config.cpp reading config.json and
// state.json (both on the SD card, both editable by hand), in a temporary
// folder (paths::data_dir() is ./playshelf_data on the host). The input is
// config.json once, with no state.json (the versions before it), then
// state.json next to a valid config.json. Beyond the sanitizers: whatever was
// read, what config::snapshot() writes reads back to the same settings
// (sanitize() keeps nothing a second read would change).
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "fuzz.h"
#include "util/config.hpp"
#include "util/paths.hpp"

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    // In memory when there is a tmpfs: every save syncs its file (paths.cpp),
    // which on a disk leaves a few runs a second.
    char shm[] = "/dev/shm/playshelf_fuzz_config_XXXXXX", tmp[] = "/tmp/playshelf_fuzz_config_XXXXXX";
    const char* dir = mkdtemp(shm);
    if (!dir) dir = mkdtemp(tmp);
    if (!dir || chdir(dir) != 0) abort();
    return 0;
}

// The settings read, saved, and read again: the same files.
static void round_trip()
{
    config::Snapshot first;
    FUZZ_ASSERT(config::snapshot(first));
    FUZZ_ASSERT(config::write(first));
    config::load();
    FUZZ_ASSERT(config::loaded() == config::Loaded::Read);
    config::Snapshot second;
    FUZZ_ASSERT(config::snapshot(second));
    if (second.shared != first.shared || second.system != first.system) {
        std::fprintf(stderr, "first:\n%s%s\nsecond:\n%s%s\n", first.shared.c_str(), first.system.c_str(),
                     second.shared.c_str(), second.system.c_str());
        FUZZ_ASSERT(!"a saved config reads back the same");
    }
}

static void clear()
{
    for (const std::string& f : { paths::config_file(), paths::state_file() })
        for (const char* suffix : { "", ".tmp", ".bad", ".bad.1" }) std::remove((f + suffix).c_str());
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string text(reinterpret_cast<const char*>(data), size);

    clear();
    FUZZ_ASSERT(paths::atomic_write(paths::config_file(), text));
    config::load();
    if (config::loaded() != config::Loaded::Stuck) round_trip();

    clear();
    FUZZ_ASSERT(paths::atomic_write(paths::config_file(), "{}"));
    FUZZ_ASSERT(paths::atomic_write(paths::state_file(), text));
    config::load();
    if (config::loaded() != config::Loaded::Stuck) round_trip();
    return 0;
}
