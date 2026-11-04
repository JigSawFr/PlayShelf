// libFuzzer target: source/install/compressed_storage.cpp's reader on any
// table. The input is an entry count (4 bytes), the size of the data the
// table points into (4 bytes), then the table. A table it accepts reads: any
// range inside the RomFS either comes out or fails cleanly (the fetch hands
// zeros, the decompressor fills what it is asked for), and a range past the
// end is refused.
#include <cstring>

#include "fuzz.h"
#include "install/compressed_storage.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    if (size < 8) return 0;
    const int32_t  entries = (int32_t)le32(data);
    const uint64_t data_size = le32(data + 4);
    auto           r = cstore::Reader::open({data + 8, size - 8}, entries, data_size, [](std::span<const uint8_t> in, std::span<uint8_t> out) {
        if (in.empty()) return false;
        std::memset(out.data(), 0x5a, out.size());
        return true;
    });
    if (!r) return 0;
    const auto fetch = [&](uint64_t off, std::span<uint8_t> out) {
        FUZZ_ASSERT(off <= data_size && out.size() <= data_size - off);   // never asked for bytes outside the data
        std::memset(out.data(), 0, out.size());
        return true;
    };
    const uint64_t total = r->virtual_size();
    std::vector<uint8_t> buf(1 << 16);
    for (uint64_t off = 0; off < total && off < (1u << 22); off += 4099) {
        const size_t n = (size_t)std::min<uint64_t>(buf.size(), total - off);
        FUZZ_ASSERT(r->read(off, {buf.data(), n}, fetch));
    }
    FUZZ_ASSERT(!r->read(total, {buf.data(), 1}, fetch));
    return 0;
}
