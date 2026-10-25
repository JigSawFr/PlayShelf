// libFuzzer target: source/util/zip_read.cpp on any archive (a GitHub
// Actions artifact, downloaded). The input is written to a file of a
// temporary folder and its "playshelf.nro" extracted, as dev_builds does.
// Beyond the sanitizers: an extraction that succeeds leaves a file of at most
// the size allowed, and one that fails leaves none and says why.
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "fuzz.h"
#include "util/zip_read.hpp"

static std::string s_dir;

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    char dir[] = "/tmp/playshelf_fuzz_zip_XXXXXX";
    if (!mkdtemp(dir)) abort();
    s_dir = dir;
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string zip = s_dir + "/a.zip", out = s_dir + "/playshelf.nro";
    // Read and written by this process alone (0600).
    const int fd = open(zip.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    FUZZ_ASSERT(fd >= 0);
    FILE* f = fdopen(fd, "wb");
    FUZZ_ASSERT(f != nullptr);
    FUZZ_ASSERT(std::fwrite(data, 1, size, f) == size);
    std::fclose(f);

    for (uint64_t max : { (uint64_t)1 << 20, (uint64_t)16 }) {
        std::string error;
        const bool ok = zip_read::extract(zip, "playshelf.nro", out, max, &error);
        struct stat st;
        const bool there = stat(out.c_str(), &st) == 0;
        if (ok) {
            FUZZ_ASSERT(there && (uint64_t)st.st_size <= max);
        } else {
            FUZZ_ASSERT(!error.empty());
            FUZZ_ASSERT(!there);   // "out_path is then removed"
        }
        std::remove(out.c_str());
    }
    return 0;
}
