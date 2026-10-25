// libFuzzer target: source/install/cnmt.cpp on any bytes, a title's content
// meta. Beyond the sanitizers: a parsed one gives an install record whose
// size is what its counts say, with the counts it was built from.
#include "fuzz.h"
#include "install/bytes.hpp"
#include "install/cnmt.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    const auto m = cnmt_parse(data, size);
    if (!m) return 0;
    FUZZ_ASSERT(m->installable().size() <= m->contents.size());
    ContentInfo meta;
    meta.size = size;
    const auto rec = cnmt_install_record(*m, meta, size % 2);
    const size_t n = m->installable().size() + 1;
    FUZZ_ASSERT(rec.size() == 8 + m->extended_header.size() + n * 0x18 + m->metas.size() * 0x10 + m->extended_data.size());
    FUZZ_ASSERT(le16(rec.data() + 2) == (uint16_t)n);
    (void)m->application_id();
    (void)m->required_system_version();
    return 0;
}
