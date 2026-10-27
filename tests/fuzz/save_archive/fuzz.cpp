// libFuzzer target: saves/archive.cpp reading any bytes as a zip and
// writing it out. Beyond the sanitizers: nothing is written outside the
// folder it is written to (every name it takes is safe_name's), and the
// folder is emptied after each run.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "fuzz.h"
#include "saves/archive.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static std::string root;
    if (root.empty()) {
        char tmpl[] = "/tmp/save_archive_fuzz.XXXXXX";
        root = mkdtemp(tmpl) ? tmpl : "";
        FUZZ_ASSERT(!root.empty());
    }
    const std::string zip = root + "/in.zip", out = root + "/out";
    FILE* f = std::fopen(zip.c_str(), "wb");
    FUZZ_ASSERT(f);
    std::fwrite(data, 1, size, f);
    std::fclose(f);
    std::vector<save_archive::Entry> entries;
    if (save_archive::read_entries(zip, &entries, nullptr))
        for (const save_archive::Entry& e : entries)
            FUZZ_ASSERT(save_archive::safe_name(e.name));
    ::mkdir(out.c_str(), 0755);
    std::string why;
    save_archive::unzip_to(zip, out, nullptr, nullptr, &why);
    save_archive::clear_dir(out, &why);
    // Only the archive and its folder there.
    FUZZ_ASSERT(save_archive::clear_dir(out, &why));
    return 0;
}
