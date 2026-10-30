// libFuzzer target: source/install/nca.cpp on any bytes, an NCA from a
// server: read whole as a .cnmt.nca with test keys (the input's header
// decrypted with them, its section with the key its key area gives), its
// first 0xC00 bytes also parsed as a decrypted header and their signature
// checked. Beyond the sanitizers: a parsed header's sections lie within
// the size it gives; a content meta read is no longer than the input.
#include <cstring>

#include "fuzz.h"
#include "install/aes.hpp"
#include "install/nca.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    static const uint8_t KAEK[16] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                                     0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
    NcaKeys keys;
    for (int i = 0; i < 32; i++)
        keys.header_key[i] = (uint8_t)(i + 1);
    keys.unwrap = [](uint8_t generation, uint8_t index, const uint8_t in[16], uint8_t out[16]) {
        if (generation > 4 || index != 0) return false;
        std::memcpy(out, in, 16);
        aes128_ecb_decrypt(KAEK, {out, 16});
        return true;
    };
    if (const auto cnmt = nca_read_cnmt({data, size}, keys)) FUZZ_ASSERT(cnmt->size() <= size);
    if (size >= NCA_HEADER) {
        if (const auto h = nca_parse_header(data)) {
            for (const NcaSection& s : h->sections)
                if (s.present) FUZZ_ASSERT(s.start <= s.end && s.end <= h->size);
            (void)nca_ctr_key(*h, keys);
        }
        (void)nca_check_signature(data);
        uint8_t copy[NCA_HEADER];
        std::memcpy(copy, data, NCA_HEADER);
        (void)nca_make_download(copy, keys.header_key.data());
    }
    return 0;
}
