// libFuzzer target: source/install/ivfc.cpp on any bytes taken as a file
// system header (zero-filled to 0x200), the integrity tree of a RomFS
// section. Beyond the sanitizers: a header that parses writes back to one that
// parses to the same layout.
#include <cstring>

#include "fuzz.h"
#include "install/ivfc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    uint8_t fs[0x200] = {};
    std::memcpy(fs, data, size < sizeof fs ? size : sizeof fs);
    const auto l = ivfc::read_fs_header(fs);
    if (!l) return 0;

    uint8_t again[0x200] = {};
    ivfc::write_fs_header(again, *l);
    const auto m = ivfc::read_fs_header(again);
    FUZZ_ASSERT(m.has_value());
    FUZZ_ASSERT(m->offset == l->offset && m->size == l->size && m->order == l->order && m->master == l->master);
    (void)ivfc::hash_levels_size(l->size[5]);
    return 0;
}
