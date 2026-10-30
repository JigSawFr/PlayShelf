// libFuzzer target: source/install/content_stream.cpp on any bytes, a .nca
// or a .ncz from a server, with the suite's test header key, fed in pieces
// whose sizes the input's first byte picks. Beyond the sanitizers: the
// bytes written are contiguous from 0 and never more than the size the
// header gave; a stream that finishes wrote exactly that much.
#include <cstring>

#include "fuzz.h"
#include "install/content_stream.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    if (size < 1) return 0;
    const size_t piece = 1 + data[0] * 97;
    data++;
    size--;
    uint8_t key[32];
    for (int i = 0; i < 32; i++)
        key[i] = (uint8_t)(i + 1);
    uint64_t header_size = 0, next = 0;
    ContentStream cs(
        key, std::nullopt, 0,
        [&](const NcaHeader& h, NcaSignature, std::string*) {
            header_size = h.size;
            return h.size <= (64u << 20);   // as a caller would bound its placeholder
        },
        [&](uint64_t offset, const uint8_t*, size_t n) {
            FUZZ_ASSERT(offset == next);
            next += n;
            FUZZ_ASSERT(next <= header_size);
            return true;
        });
    bool ok = true;
    for (size_t at = 0; ok && at < size; at += piece)
        ok = cs.feed({data + at, std::min(piece, size - at)});
    if (ok && cs.finish()) FUZZ_ASSERT(next == header_size);
    FUZZ_ASSERT(cs.written() == next);
    return 0;
}
