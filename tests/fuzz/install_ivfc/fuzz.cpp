// libFuzzer target: source/install/ivfc.cpp on any bytes, a section's file
// system header (padded or cut to 0x200 bytes). Beyond the sanitizers: a
// header that parses gives levels that lie in order within the section, and
// written back it reads again as the same layout.
#include <array>
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
    for (size_t i = 0; i < ivfc::LEVELS; i++) {
        FUZZ_ASSERT(l->order[i] >= 1 && l->order[i] < 64);
        FUZZ_ASSERT(l->offset[i] + l->size[i] >= l->offset[i]);   // no wrap
    }
    uint8_t again[0x200] = {};
    ivfc::write_fs_header(again, *l);
    const auto m = ivfc::read_fs_header(again);
    FUZZ_ASSERT(m.has_value());
    FUZZ_ASSERT(m->offset == l->offset && m->size == l->size && m->order == l->order && m->master == l->master);
    return 0;
}
