// libFuzzer target: source/install/recompress.cpp on any bytes, a Program NCA
// as the console stores it: its header decrypted with test keys, its sections
// looked over by inspect() before a byte is made. The codec is never called
// (inspect() makes nothing); a read outside the input fails. Beyond the
// sanitizers: nothing is asked of the original past its size.
#include <cstring>
#include <optional>

#include "fuzz.h"
#include "install/aes.hpp"
#include "install/recompress.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    static const uint8_t KAEK[16] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                                     0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
    NcaKeys keys;
    for (int i = 0; i < 32; i++) keys.header_key[i] = (uint8_t)(i + 1);
    keys.unwrap = [](uint8_t generation, uint8_t index, const uint8_t in[16], uint8_t out[16]) {
        if (generation > 4 || index != 0) return false;
        std::memcpy(out, in, 16);
        aes128_ecb_decrypt(KAEK, {out, 16});
        return true;
    };
    ProgramRecompressor rc(
        [data, size](uint64_t offset, std::span<uint8_t> out) {
            if (offset > size || out.size() > size - offset) return false;
            std::memcpy(out.data(), data + offset, out.size());
            return true;
        },
        size, keys, [](std::span<const uint8_t>, std::span<uint8_t>) { return std::optional<size_t>(); },
        [](std::span<const uint8_t>, std::span<uint8_t>) { return false; });
    (void)rc.inspect();
    return 0;
}
