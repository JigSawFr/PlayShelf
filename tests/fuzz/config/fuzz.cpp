// libFuzzer target: source/util/config.cpp reading config.json (on the SD
// card, editable by hand), in a temporary folder (paths::data_dir() is
// ./playshelf_data on the host). Beyond the sanitizers: whatever was read,
// what config::snapshot() writes reads back to the same settings
// (sanitize() keeps nothing a second read would change), and a newer
// schema's file is never written over.
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

// The settings read, saved, and read again: the same file.
static void round_trip()
{
    config::Snapshot first;
    FUZZ_ASSERT(config::snapshot(first));
    FUZZ_ASSERT(config::write(first));
    config::load();
    FUZZ_ASSERT(config::loaded() == config::Loaded::Read);
    config::Snapshot second;
    FUZZ_ASSERT(config::snapshot(second));
    if (second.shared != first.shared) {
        std::fprintf(stderr, "first:\n%s\nsecond:\n%s\n", first.shared.c_str(), second.shared.c_str());
        FUZZ_ASSERT(!"a saved config reads back the same");
    }
}

static void clear()
{
    for (const char* suffix : { "", ".tmp", ".bad", ".bad.1" }) std::remove((paths::config_file() + suffix).c_str());
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string text(reinterpret_cast<const char*>(data), size);
    clear();
    FUZZ_ASSERT(paths::atomic_write(paths::config_file(), text));
    config::load();
    // A newer schema's file: nothing written over it.
    if (config::loaded() == config::Loaded::Newer) {
        config::Snapshot none;
        FUZZ_ASSERT(!config::snapshot(none));
        std::string kept;
        FUZZ_ASSERT(paths::read_file(paths::config_file(), kept) && kept == text);
    } else if (config::loaded() != config::Loaded::Stuck) {
        round_trip();
    }
    return 0;
}
