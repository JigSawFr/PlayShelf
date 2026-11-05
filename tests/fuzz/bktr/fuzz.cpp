// libFuzzer target: source/install/bktr.cpp on any bytes, the patch info of a
// file system header and the bucket trees of an update's RomFS section. Beyond
// the sanitizers: a tree that parses is flat, starts at 0, is strictly
// ordered and ends past its last entry.
#include <cstring>

#include "fuzz.h"
#include "install/bktr.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    if (size < 9) return 0;
    const uint8_t mode = data[0];
    uint64_t section = 0;
    std::memcpy(&section, data + 1, 8);
    data += 9;
    size -= 9;

    if (mode & 1) {
        uint8_t fs[0x200] = {};
        std::memcpy(fs, data, size < sizeof fs ? size : sizeof fs);
        (void)bktr::read_patch_info(fs, section);
        return 0;
    }
    const size_t entry = (mode & 2) ? bktr::SUBSECTION_ENTRY : bktr::RELOCATION_ENTRY;
    const int32_t count = (int32_t)(section & 0xFFFF) - 8;
    const auto t = bktr::parse_tree({data, size}, count, entry);
    if (!t) return 0;
    FUZZ_ASSERT(t->count() == (size_t)count);
    FUZZ_ASSERT(t->offset(0) == 0);
    for (size_t i = 1; i < t->count(); i++) FUZZ_ASSERT(t->offset(i) > t->offset(i - 1));
    FUZZ_ASSERT(t->end > (uint64_t)t->offset(t->count() - 1));
    return 0;
}
