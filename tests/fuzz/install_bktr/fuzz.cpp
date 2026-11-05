// libFuzzer target: source/install/bktr.cpp on any bytes, the patch tables of
// an update's RomFS section. The input is a 0x200-byte file system header, the
// section's size (8 bytes), an entry count (4 bytes), which kind of table (1
// byte), then the table's bytes. Beyond the sanitizers: a patch info that
// parses lies within the section; a tree that parses holds as many entries as
// it was told, of the size asked, in the order they are stored.
#include <algorithm>
#include <span>

#include "fuzz.h"
#include "install/bytes.hpp"
#include "install/bktr.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    constexpr size_t FS = 0x200, FIXED = FS + 8 + 4 + 1;
    if (size < FIXED) return 0;

    const uint64_t section_size = le64(data + FS);
    if (const auto p = bktr::read_patch_info(data, section_size)) {
        FUZZ_ASSERT(p->has_indirect() && p->has_aes());
        FUZZ_ASSERT(p->indirect_offset <= section_size && p->indirect_size <= section_size - p->indirect_offset);
        FUZZ_ASSERT(p->aes_offset <= section_size);
        FUZZ_ASSERT(p->indirect_offset + p->indirect_size <= p->aes_offset);
    }

    const int32_t entry_count = (int32_t)le32(data + FS + 8);
    const size_t  entry_size = (data[FS + 12] & 1) ? bktr::SUBSECTION_ENTRY : bktr::RELOCATION_ENTRY;
    if (const auto t = bktr::parse_tree(std::span<const uint8_t>(data + FIXED, size - FIXED), entry_count, entry_size)) {
        FUZZ_ASSERT(t->entry_size == entry_size);
        FUZZ_ASSERT(t->count() == (size_t)entry_count);
        FUZZ_ASSERT(t->end > 0);
        for (size_t i = 1; i < t->count(); i++) FUZZ_ASSERT(t->offset(i - 1) <= t->offset(i));
    }
    return 0;
}
