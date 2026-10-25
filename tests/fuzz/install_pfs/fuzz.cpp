// libFuzzer target: source/install/pfs.cpp on any bytes, an NSP's or a game
// card partition's header from a server. Beyond the sanitizers: a parsed
// archive's every entry lies past its header and, the archive's size being
// the input's, within it; the header size it reports is the one
// pfs_header_size gave. The same bytes as a game card header, too.
#include "fuzz.h"
#include "install/pfs.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    const auto hs = pfs_header_size(data, size);
    const auto p = pfs_parse(data, size, size);
    if (p) {
        FUZZ_ASSERT(hs && hs->size == p->header_size && hs->kind == p->kind);
        FUZZ_ASSERT(p->header_size <= size);
        for (const PfsEntry& e : p->entries) {
            FUZZ_ASSERT(!e.name.empty());
            FUZZ_ASSERT(e.offset >= p->header_size && e.offset + e.size <= size);
            FUZZ_ASSERT(e.hashed_size <= e.size);
        }
        (void)p->ending_with(".nca");
    }
    // Without the archive's size: only the header bounds the entries.
    (void)pfs_parse(data, size, 0);
    if (const auto x = xci_root(data, size, size)) FUZZ_ASSERT(x->offset + x->header_size <= size);
    return 0;
}
